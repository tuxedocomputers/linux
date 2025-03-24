// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * This driver implements the ACPI TUXI device found on some TUXEDO notebooks.

 * Copyright (C) 2024-2025 Werner Sembach wse@tuxedocomputers.com
 */

#include <linux/acpi.h>
#include <linux/cleanup.h>
#include <linux/device.h>
#include <linux/hwmon.h>
#include <linux/limits.h>
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/units.h>
#include <linux/workqueue.h>

#define TUXI_SAFEGUARD_PERIOD 1000      // 1s
#define TUXI_PWM_FAN_ON_MIN_SPEED 0x40  // ~25%
#define TUXI_TEMP_LEVEL_HYSTERESIS 1500 // 1.5°C
#define TUXI_FW_TEMP_OFFSET 2730        // Kelvin to Celsius
#define TUXI_MAX_FAN_COUNT 16           /*
					 * If this is increased, new lines must
					 * be added to hwmcinfo below.
					 */

static const struct acpi_device_id acpi_device_ids[] = {
	{"TUXI0000", 0},
	{"", 0}
};
MODULE_DEVICE_TABLE(acpi, acpi_device_ids);

struct tux_driver_data_t {
	acpi_handle tfan_handle;
	struct device *hwmdev;
};

struct tux_hwmon_driver_data_t {
	struct delayed_work work;
	struct device *hwmdev;
	u8 fan_count;
	const char *fan_types[TUXI_MAX_FAN_COUNT];
	u8 temp_level[TUXI_MAX_FAN_COUNT];
	u8 curr_speed[TUXI_MAX_FAN_COUNT];
	u8 want_speed[TUXI_MAX_FAN_COUNT];
	u8 pwm_enabled;
};

struct tux_temp_high_config_t {
	long temp;
	u8 min_speed;
};

/*
 * Speed values in this table must be >= TUXI_PWM_FAN_ON_MIN_SPEED to avoid
 * undefined behaviour.
 */
static const struct tux_temp_high_config_t temp_levels[] = {
	{  80000, 0x4d }, // ~30%
	{  90000, 0x66 }, // ~40%
	{ 100000, 0xff }, // 100%
	{ }
};

/*
 * Set fan speed target
 *
 * Set a specific fan speed (needs manual mode)
 *
 * Arg0: Fan index
 * Arg1: Fan speed as a fraction of maximum speed (0-255)
 */
#define TUXI_TFAN_METHOD_SET_FAN_SPEED		"SSPD"

/*
 * Get fan speed target
 *
 * Arg0: Fan index
 * Returns: Current fan speed target a fraction of maximum speed (0-255)
 */
#define TUXI_TFAN_METHOD_GET_FAN_SPEED		"GSPD"

/*
 * Get fans count
 *
 * Returns: Number of individually controllable fans
 */
#define TUXI_TFAN_METHOD_GET_FAN_COUNT		"GCNT"

/*
 * Set fans mode
 *
 * Arg0: 0 = auto, 1 = manual
 */
#define TUXI_TFAN_METHOD_SET_FAN_MODE		"SMOD"

/*
 * Get fans mode
 *
 * Returns: 0 = auto, 1 = manual
 */
#define TUXI_TFAN_METHOD_GET_FAN_MODE		"GMOD"

#define TUXI_TFAN_FAN_MODE_AUTO 0
#define TUXI_TFAN_FAN_MODE_MANUAL 1

/*
 * Get fan type/what the fan is pointed at
 *
 * Arg0: Fan index
 * Returns: 0 = general, 1 = CPU, 2 = GPU
 */
#define TUXI_TFAN_METHOD_GET_FAN_TYPE		"GTYP"

static const char * const tux_fan_type_labels[] = {
	"general",
	"cpu",
	"gpu"
};

/*
 * Get fan temperature/temperature of what the fan is pointed at
 *
 * Arg0: Fan index
 * Returns: Temperature sensor value in 10ths of degrees kelvin
 */
#define TUXI_TFAN_METHOD_GET_FAN_TEMPERATURE	"GTMP"

/*
 * Get actual fan speed in RPM
 *
 * Arg0: Fan index
 * Returns: Speed sensor value in revolutions per minute
 */
#define TUXI_TFAN_METHOD_GET_FAN_RPM		"GRPM"

static int tux_tfan_method(struct acpi_device *device, acpi_string method,
			   unsigned long long *params, u32 pcount,
			   unsigned long long *retval)
{
	struct tux_driver_data_t *driver_data = dev_get_drvdata(&device->dev);
	acpi_handle handle = driver_data->tfan_handle;
	union acpi_object *obj __free(kfree) = NULL;
	struct acpi_object_list arguments;
	unsigned long long data;
	acpi_status status;
	unsigned int i;

	if (pcount > 0) {
		obj = kcalloc(pcount, sizeof(*arguments.pointer), GFP_KERNEL);

		arguments.count = pcount;
		arguments.pointer = obj;
		for (i = 0; i < pcount; ++i) {
			arguments.pointer[i].type = ACPI_TYPE_INTEGER;
			arguments.pointer[i].integer.value = params[i];
		}
	}
	status = acpi_evaluate_integer(handle, method,
				       pcount ? &arguments : NULL, &data);
	if (ACPI_FAILURE(status))
		return_ACPI_STATUS(status);

	if (retval)
		*retval = data;

	return 0;
}

static umode_t tux_hwm_is_visible(const void *data, enum hwmon_sensor_types type,
				  u32 attr __always_unused, int channel)
{
	struct tux_hwmon_driver_data_t const *driver_data = data;

	if (channel >= driver_data->fan_count)
		return 0;

	switch (type) {
	case hwmon_fan:
		return 0444;
	case hwmon_pwm:
		return 0644;
	case hwmon_temp:
		return 0444;
	default:
		break;
	}

	return -EOPNOTSUPP;
}

static int tux_hwm_read(struct device *dev, enum hwmon_sensor_types type, u32 attr,
			int channel, long *val)
{
	struct tux_hwmon_driver_data_t *driver_data = dev_get_drvdata(dev);
	struct acpi_device *pdev = to_acpi_device(dev->parent);
	unsigned long long params[2], retval;
	int ret;

	switch (type) {
	case hwmon_fan:
		params[0] = channel;
		ret = tux_tfan_method(pdev, TUXI_TFAN_METHOD_GET_FAN_RPM,
				      params, 1, &retval);
		*val = retval > S32_MAX ? S32_MAX : retval;
		return ret;
	case hwmon_pwm:
		switch (attr) {
		case hwmon_pwm_input:
			if (driver_data->pwm_enabled) {
				*val = driver_data->curr_speed[channel];
				return 0;
			}
			params[0] = channel;
			ret = tux_tfan_method(pdev,
					      TUXI_TFAN_METHOD_GET_FAN_SPEED,
					      params, 1, &retval);
			*val = retval > S32_MAX ? S32_MAX : retval;
			return ret;
		case hwmon_pwm_enable:
			*val = driver_data->pwm_enabled;
			return ret;
		}
		break;
	case hwmon_temp:
		params[0] = channel;
		ret = tux_tfan_method(pdev, TUXI_TFAN_METHOD_GET_FAN_TEMPERATURE,
				      params, 1, &retval);
		*val = retval > S32_MAX / 100 ?
			S32_MAX : (retval - TUXI_FW_TEMP_OFFSET) * 100;
		return ret;
	default:
		break;
	}

	return -EOPNOTSUPP;
}

static int tux_hwm_read_string(struct device *dev,
			       enum hwmon_sensor_types type __always_unused,
			       u32 attr __always_unused, int channel,
			       const char **str)
{
	struct tux_hwmon_driver_data_t *driver_data = dev_get_drvdata(dev);

	*str = driver_data->fan_types[channel];

	return 0;
}

static int tux_write_speed(struct device *dev, int channel, u8 val, bool force)
{
	struct tux_hwmon_driver_data_t *driver_data = dev_get_drvdata(dev);
	struct acpi_device *pdev = to_acpi_device(dev->parent);
	unsigned long long new_speed, params[2];
	u8 temp_level;
	int ret;

	params[0] = channel;

	/*
	 * The heatpipe across the VRMs is shared between both fans and the VRMs
	 * are the most likely to go up in smoke. So it's better to apply the
	 * minimum fan speed to all fans if either CPU or GPU is working hard.
	 */
	temp_level = max_array(driver_data->temp_level, driver_data->fan_count);
	if (temp_level)
		new_speed = max(val, temp_levels[temp_level - 1].min_speed);
	else if (val < TUXI_PWM_FAN_ON_MIN_SPEED / 2)
		new_speed = 0;
	else if (val < TUXI_PWM_FAN_ON_MIN_SPEED)
		new_speed = TUXI_PWM_FAN_ON_MIN_SPEED;
	else
		new_speed = val;

	if (force || new_speed != driver_data->curr_speed[channel]) {
		params[0] = channel;
		params[1] = new_speed;
		ret = tux_tfan_method(pdev, TUXI_TFAN_METHOD_SET_FAN_SPEED,
				      params, 2, NULL);
		if (ret)
			return ret;
	}

	driver_data->curr_speed[channel] = new_speed;

	return 0;
}

static int tux_hwm_write(struct device *dev,
			 enum hwmon_sensor_types type __always_unused, u32 attr,
			 int channel, long val)
{
	struct tux_hwmon_driver_data_t *driver_data = dev_get_drvdata(dev);
	struct acpi_device *pdev = to_acpi_device(dev->parent);
	unsigned long long params[2];
	unsigned int i;
	int ret;

	switch (attr) {
	case hwmon_pwm_input:
		if (val > U8_MAX || val < 0)
			return -EINVAL;

		if (driver_data->pwm_enabled) {
			driver_data->want_speed[channel] = val;
			return tux_write_speed(dev, channel, val, false);
		}

		return 0;
	case hwmon_pwm_enable:
		params[0] = val ? TUXI_TFAN_FAN_MODE_MANUAL :
				  TUXI_TFAN_FAN_MODE_AUTO;
		ret = tux_tfan_method(pdev, TUXI_TFAN_METHOD_SET_FAN_MODE,
				      params, 1, NULL);
		if (ret)
			return ret;

		driver_data->pwm_enabled = val;

		/*
		 * Activating PWM sets speed to 0. Alternative design decision
		 * could be to keep the current value. This would require proper
		 * setting of driver_data->curr_speed for example.
		 */
		if (val)
			for (i = 0; i < driver_data->fan_count; ++i) {
				ret = tux_write_speed(dev, i, 0, true);
				if (ret)
					return ret;
			}

		return 0;
	}

	return -EOPNOTSUPP;
}

static const struct hwmon_ops tux_hwmops = {
	.is_visible = tux_hwm_is_visible,
	.read = tux_hwm_read,
	.read_string = tux_hwm_read_string,
	.write = tux_hwm_write,
};

static const struct hwmon_channel_info * const tux_hwmcinfo[] = {
	HWMON_CHANNEL_INFO(fan,
			   HWMON_F_INPUT | HWMON_F_LABEL,
			   HWMON_F_INPUT | HWMON_F_LABEL,
			   HWMON_F_INPUT | HWMON_F_LABEL,
			   HWMON_F_INPUT | HWMON_F_LABEL,
			   HWMON_F_INPUT | HWMON_F_LABEL,
			   HWMON_F_INPUT | HWMON_F_LABEL,
			   HWMON_F_INPUT | HWMON_F_LABEL,
			   HWMON_F_INPUT | HWMON_F_LABEL,
			   HWMON_F_INPUT | HWMON_F_LABEL,
			   HWMON_F_INPUT | HWMON_F_LABEL,
			   HWMON_F_INPUT | HWMON_F_LABEL,
			   HWMON_F_INPUT | HWMON_F_LABEL,
			   HWMON_F_INPUT | HWMON_F_LABEL,
			   HWMON_F_INPUT | HWMON_F_LABEL,
			   HWMON_F_INPUT | HWMON_F_LABEL,
			   HWMON_F_INPUT | HWMON_F_LABEL),
	HWMON_CHANNEL_INFO(pwm,
			   HWMON_PWM_INPUT | HWMON_PWM_ENABLE,
			   HWMON_PWM_INPUT | HWMON_PWM_ENABLE,
			   HWMON_PWM_INPUT | HWMON_PWM_ENABLE,
			   HWMON_PWM_INPUT | HWMON_PWM_ENABLE,
			   HWMON_PWM_INPUT | HWMON_PWM_ENABLE,
			   HWMON_PWM_INPUT | HWMON_PWM_ENABLE,
			   HWMON_PWM_INPUT | HWMON_PWM_ENABLE,
			   HWMON_PWM_INPUT | HWMON_PWM_ENABLE,
			   HWMON_PWM_INPUT | HWMON_PWM_ENABLE,
			   HWMON_PWM_INPUT | HWMON_PWM_ENABLE,
			   HWMON_PWM_INPUT | HWMON_PWM_ENABLE,
			   HWMON_PWM_INPUT | HWMON_PWM_ENABLE,
			   HWMON_PWM_INPUT | HWMON_PWM_ENABLE,
			   HWMON_PWM_INPUT | HWMON_PWM_ENABLE,
			   HWMON_PWM_INPUT | HWMON_PWM_ENABLE,
			   HWMON_PWM_INPUT | HWMON_PWM_ENABLE),
	HWMON_CHANNEL_INFO(temp,
			   HWMON_T_INPUT | HWMON_T_LABEL,
			   HWMON_T_INPUT | HWMON_T_LABEL,
			   HWMON_T_INPUT | HWMON_T_LABEL,
			   HWMON_T_INPUT | HWMON_T_LABEL,
			   HWMON_T_INPUT | HWMON_T_LABEL,
			   HWMON_T_INPUT | HWMON_T_LABEL,
			   HWMON_T_INPUT | HWMON_T_LABEL,
			   HWMON_T_INPUT | HWMON_T_LABEL,
			   HWMON_T_INPUT | HWMON_T_LABEL,
			   HWMON_T_INPUT | HWMON_T_LABEL,
			   HWMON_T_INPUT | HWMON_T_LABEL,
			   HWMON_T_INPUT | HWMON_T_LABEL,
			   HWMON_T_INPUT | HWMON_T_LABEL,
			   HWMON_T_INPUT | HWMON_T_LABEL,
			   HWMON_T_INPUT | HWMON_T_LABEL,
			   HWMON_T_INPUT | HWMON_T_LABEL),
	NULL
};

static const struct hwmon_chip_info tux_hwminfo = {
	.ops = &tux_hwmops,
	.info = tux_hwmcinfo
};

static u8 tux_get_temp_level(struct tux_hwmon_driver_data_t *driver_data,
			     u8 fan_id, long temp)
{
	long temp_low, temp_high;
	unsigned int i;
	int ret;

	ret = driver_data->temp_level[fan_id];

	for (i = 0; temp_levels[i].temp; ++i) {
		temp_low = i == 0 ? S32_MIN : temp_levels[i - 1].temp;
		temp_high = temp_levels[i].temp;
		if (ret > i)
			temp_high -= TUXI_TEMP_LEVEL_HYSTERESIS;

		if (temp >= temp_low && temp < temp_high)
			return i;
	}
	if (temp >= temp_high)
		ret = i;

	return ret;
}

static void tux_periodic_hw_safeguard(struct work_struct *work)
{
	struct tux_hwmon_driver_data_t *driver_data = container_of(work,
								   struct tux_hwmon_driver_data_t,
								   work.work);
	struct device *dev = driver_data->hwmdev;
	struct acpi_device *pdev = to_acpi_device(dev->parent);
	unsigned long long params[2], retval;
	unsigned int i;
	long temp;
	int ret;

	for (i = 0; i < driver_data->fan_count; ++i) {
		params[0] = i;
		ret = tux_tfan_method(pdev, TUXI_TFAN_METHOD_GET_FAN_TEMPERATURE,
				      params, 1, &retval);
		/*
		 * If reading the temperature fails, default to a high value to
		 * be on the safe side in the worst case.
		 */
		if (ret)
			retval = TUXI_FW_TEMP_OFFSET + 1200;

		temp = retval > S32_MAX / 100 ?
			S32_MAX : (retval - TUXI_FW_TEMP_OFFSET) * 100;

		driver_data->temp_level[i] = tux_get_temp_level(driver_data, i,
								temp);
	}

	// Reapply want_speeds to respect eventual new temp_levels
	for (i = 0; i < driver_data->fan_count; ++i)
		tux_write_speed(dev, i, driver_data->want_speed[i], false);

	schedule_delayed_work(&driver_data->work, TUXI_SAFEGUARD_PERIOD);
}

static int tux_hwmon_add_devices(struct acpi_device *pdev, struct device **hwmdev)
{
	struct tux_hwmon_driver_data_t *driver_data;
	unsigned long long params[2], retval;
	unsigned int i;
	int ret;

	/*
	 * The first version of TUXI TFAN didn't have the Get Fan Temperature
	 * method which is integral to this driver. So probe for existence and
	 * abort otherwise.
	 *
	 * The Get Fan Speed method is also missing in that version, but was
	 * added in the same version so it doesn't have to be probe separately.
	 */
	params[0] = 0;
	ret = tux_tfan_method(pdev, TUXI_TFAN_METHOD_GET_FAN_TEMPERATURE,
			      params, 1, &retval);
	if (ret)
		return ret;

	driver_data = devm_kzalloc(&pdev->dev, sizeof(*driver_data), GFP_KERNEL);
	if (!driver_data)
		return -ENOMEM;

	/*
	 * Loading this module sets the fan mode to auto. Alternative design
	 * decision could be to keep the current value. This would require
	 * proper initialization of driver_data->curr_speed for example.
	 */
	params[0] = TUXI_TFAN_FAN_MODE_AUTO;
	ret = tux_tfan_method(pdev, TUXI_TFAN_METHOD_SET_FAN_MODE, params, 1,
			      NULL);
	if (ret)
		return ret;

	ret = tux_tfan_method(pdev, TUXI_TFAN_METHOD_GET_FAN_COUNT, NULL, 0,
			      &retval);
	if (ret)
		return ret;
	if (retval > TUXI_MAX_FAN_COUNT)
		return -EINVAL;
	driver_data->fan_count = retval;

	for (i = 0; i < driver_data->fan_count; ++i) {
		params[0] = i;
		ret = tux_tfan_method(pdev, TUXI_TFAN_METHOD_GET_FAN_TYPE,
				      params, 1, &retval);
		if (ret)
			return ret;
		if (retval >= ARRAY_SIZE(tux_fan_type_labels))
			return -EOPNOTSUPP;
		driver_data->fan_types[i] = tux_fan_type_labels[retval];
	}

	*hwmdev = devm_hwmon_device_register_with_info(&pdev->dev,
						       "tuxedo_nbxx_acpi_tuxi",
						       driver_data, &tux_hwminfo,
						       NULL);
	if (IS_ERR(*hwmdev))
		return PTR_ERR(*hwmdev);

	driver_data->hwmdev = *hwmdev;

	INIT_DELAYED_WORK(&driver_data->work, tux_periodic_hw_safeguard);
	schedule_delayed_work(&driver_data->work, TUXI_SAFEGUARD_PERIOD);

	return 0;
}

static void tux_hwmon_remove_devices(struct device *hwmdev)
{
	struct tux_hwmon_driver_data_t *driver_data = dev_get_drvdata(hwmdev);
	struct acpi_device *pdev = to_acpi_device(hwmdev->parent);
	unsigned long long params[2];

	cancel_delayed_work_sync(&driver_data->work);

	params[0] = TUXI_TFAN_FAN_MODE_AUTO;
	tux_tfan_method(pdev, TUXI_TFAN_METHOD_SET_FAN_MODE, params, 1, NULL);
}

static int tux_add(struct acpi_device *device)
{
	struct tux_driver_data_t *driver_data;
	acpi_status status;

	driver_data = devm_kzalloc(&device->dev, sizeof(*driver_data),
				   GFP_KERNEL);
	if (!driver_data)
		return -ENOMEM;

	// Find subdevices
	status = acpi_get_handle(device->handle, "TFAN",
				 &driver_data->tfan_handle);
	if (ACPI_FAILURE(status))
		return_ACPI_STATUS(status);

	dev_set_drvdata(&device->dev, driver_data);

	return tux_hwmon_add_devices(device, &driver_data->hwmdev);
}

static void tux_remove(struct acpi_device *device)
{
	struct tux_driver_data_t *driver_data = dev_get_drvdata(&device->dev);

	tux_hwmon_remove_devices(driver_data->hwmdev);
}

static struct acpi_driver acpi_driver = {
	.name = "tuxedo_nbxx_acpi_tuxi",
	.ids = acpi_device_ids,
	.ops = {
		.add = tux_add,
		.remove = tux_remove,
	},
};

module_acpi_driver(acpi_driver);

MODULE_DESCRIPTION("Fan control for TUXEDO devices using the TUXI ACPI device");
MODULE_AUTHOR("Werner Sembach <wse@tuxedocomputers.com>");
MODULE_LICENSE("GPL");
