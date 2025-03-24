// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * This driver implements some hotkeys found TUXEDO notebooks with board vendor
 * NB04.
 *
 * Copyright (c) 2023-2024 Christoffer Sandberg <cs@tuxedo.de>
 * Copyright (c) 2024-2025 Werner Sembach <wse@tuxedocomputers.com>
 */

#include <linux/dmi.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/wmi.h>

static const struct wmi_device_id tuxedo_nb04_wmi_notify_device_ids[] = {
	{ .guid_string = "96A786FA-690C-48FB-9EB3-FA9BC3D92300" },
	{ }
};
MODULE_DEVICE_TABLE(wmi, tuxedo_nb04_wmi_notify_device_ids);

struct tux_driver_data_t {
	struct input_dev *idev;
};

enum tux_wmi_notify_event {
	tux_wmi_notify_event_touchpad			= 0x06,
	tux_wmi_notify_event_microphone_mute_switch	= 0x07,
	tux_wmi_notify_event_kb_brightness_up		= 0x08,
	tux_wmi_notify_event_kb_brightness_down		= 0x09
};

static struct key_entry tux_key_map[] = {
	{ KE_KEY, tux_wmi_notify_event_microphone_mute_switch,	{ KEY_F20 } },
	{ KE_KEY, tux_wmi_notify_event_touchpad,		{ KEY_F21 } },
	{ KE_KEY, tux_wmi_notify_event_kb_brightness_down,	{ KEY_KBDILLUMDOWN } },
	{ KE_KEY, tux_wmi_notify_event_kb_brightness_up,	{ KEY_KBDILLUMUP } },
	{ KE_END, 0 }
};

static int tux_probe(struct wmi_device *wdev, const void *context __always_unused)
{
	struct tux_driver_data_t *driver_data;
	struct input_dev *idev;
	int ret;

	driver_data = devm_kzalloc(&wdev->dev, sizeof(*driver_data), GFP_KERNEL);
	if (!driver_data)
		return -ENOMEM;

	dev_set_drvdata(&wdev->dev, driver_data);

	idev = devm_input_allocate_device(&wdev->dev);
	if (!idev)
		return -ENOMEM;

	idev->name = "TUXEDO NB04 Platform Keyboard";

	ret = sparse_keymap_setup(idev, tux_key_map, NULL);
	if (ret)
		return ret;

	driver_data->idev = idev;

	return input_register_device(idev);
}

static void tux_notify(struct wmi_device *device, union acpi_object *data)
{
	struct tux_driver_data_t *driver_data = dev_get_drvdata(&device->dev);
	struct input_dev *idev = driver_data->idev;
	enum tux_wmi_notify_event event;

	if (data && data->type == ACPI_TYPE_BUFFER && data->buffer.length >= 2) {
		event = data->buffer.pointer[1];
		if (in_range(event, tux_wmi_notify_event_touchpad, 4))
			sparse_keymap_report_event(idev, event, 1, true);
	}
}

static struct wmi_driver tuxedo_nb04_wmi_notify_driver = {
	.driver = {
		.name = "tuxedo_nb04_wmi_notify",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
	.id_table = tuxedo_nb04_wmi_notify_device_ids,
	.probe = tux_probe,
	.notify = tux_notify,
	.no_singleton = true,
};

/*
 * We don't know if the WMI API is stable and how unique the GUID is for this
 * ODM. To be on the safe side we therefore only run this driver on tested
 * devices defined by this list.
 */
static const struct dmi_system_id tux_tested_devices_dmi_table[] __initconst = {
	{
		// TUXEDO Sirius 16 Gen1
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "TUXEDO"),
			DMI_EXACT_MATCH(DMI_BOARD_NAME, "APX958"),
		},
	},
	{
		// TUXEDO Sirius 16 Gen2
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "TUXEDO"),
			DMI_EXACT_MATCH(DMI_BOARD_NAME, "AHP958"),
		},
	},
	{ }
};

static int __init tuxedo_nb04_wmi_notify_init(void)
{
	if (!dmi_check_system(tux_tested_devices_dmi_table))
		return -ENODEV;

	return wmi_driver_register(&tuxedo_nb04_wmi_notify_driver);
}
module_init(tuxedo_nb04_wmi_notify_init);

static void __exit tuxedo_nb04_wmi_notify_exit(void)
{
	return wmi_driver_unregister(&tuxedo_nb04_wmi_notify_driver);
}
module_exit(tuxedo_nb04_wmi_notify_exit);

MODULE_DESCRIPTION("Virtual keyboard for hotkeys of TUXEDO NB04 devices");
MODULE_AUTHOR("Christoffer Sandberg <cs@tuxedo.de>");
MODULE_AUTHOR("Werner Sembach <wse@tuxedocomputers.com>");
MODULE_LICENSE("GPL");
