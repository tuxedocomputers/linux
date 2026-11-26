// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * ACPI backend for accessing the Uniwill embedded controller.
 *
 * Special thanks go to Pőcze Barnabás, Christoffer Sandberg and Werner Sembach
 * for supporting the development of this driver either through prior work or
 * by answering questions regarding the underlying ACPI and WMI interfaces.
 *
 * Copyright (C) 2026 Armin Wolf <W_Armin@gmx.de>
 */

#include <linux/acpi.h>
#include <linux/array_size.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/device/driver.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/limits.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/regmap.h>

#include "acpi-ec.h"

#include "core.h"

#define UNIWILL_ACPI_HID	"INOU0000"

/*
 * The OEM software always sleeps up to 6 ms after reading/writing EC
 * registers, so we emulate this behaviour for maximum compatibility.
 */
#define UNIWILL_EC_DELAY_US	6000

static int uniwill_acpi_ec_reg_write(void *context, unsigned int reg, unsigned int val)
{
	union acpi_object params[2] = {
		{
			.integer = {
				.type = ACPI_TYPE_INTEGER,
				.value = reg,
			},
		},
		{
			.integer = {
				.type = ACPI_TYPE_INTEGER,
				.value = val,
			},
		},
	};
	struct acpi_object_list input = {
		.count = ARRAY_SIZE(params),
		.pointer = params,
	};
	acpi_handle handle = context;
	acpi_status status;

	status = acpi_evaluate_object(handle, "ECRW", &input, NULL);
	if (ACPI_FAILURE(status))
		return -EIO;

	usleep_range(UNIWILL_EC_DELAY_US, UNIWILL_EC_DELAY_US * 2);

	return 0;
}

static int uniwill_acpi_ec_reg_read(void *context, unsigned int reg, unsigned int *val)
{
	union acpi_object params[1] = {
		{
			.integer = {
				.type = ACPI_TYPE_INTEGER,
				.value = reg,
			},
		},
	};
	struct acpi_object_list input = {
		.count = ARRAY_SIZE(params),
		.pointer = params,
	};
	acpi_handle handle = context;
	unsigned long long output;
	acpi_status status;

	status = acpi_evaluate_integer(handle, "ECRR", &input, &output);
	if (ACPI_FAILURE(status))
		return -EIO;

	if (output > U8_MAX)
		return -ENXIO;

	usleep_range(UNIWILL_EC_DELAY_US, UNIWILL_EC_DELAY_US * 2);

	*val = output;

	return 0;
}

static const struct regmap_bus uniwill_acpi_ec_bus = {
	.reg_write = uniwill_acpi_ec_reg_write,
	.reg_read = uniwill_acpi_ec_reg_read,
	.reg_format_endian_default = REGMAP_ENDIAN_LITTLE,
	.val_format_endian_default = REGMAP_ENDIAN_LITTLE,
};

static int uniwill_platform_probe(struct platform_device *pdev)
{
	acpi_handle handle;

	handle = ACPI_HANDLE(&pdev->dev);
	if (!handle)
		return -ENODEV;

	return uniwill_probe(&pdev->dev, &uniwill_acpi_ec_bus, handle, UNIWILL_ACPI_EC);
}

static void uniwill_platform_remove(struct platform_device *pdev)
{
	uniwill_remove(&pdev->dev, UNIWILL_ACPI_EC);
}

static void uniwill_platform_shutdown(struct platform_device *pdev)
{
	uniwill_shutdown(&pdev->dev);
}

static DEFINE_SIMPLE_DEV_PM_OPS(uniwill_platform_pm_ops, uniwill_suspend, uniwill_resume);

/*
 * We only use the DMI table for auoloading because the ACPI device itself
 * does not guarantee that the underlying EC implementation is supported.
 */
static const struct acpi_device_id uniwill_acpi_id_table[] = {
	{ UNIWILL_ACPI_HID },
	{ },
};

static struct platform_driver uniwill_platform_driver = {
	.driver = {
		.name = UNIWILL_EC_DRIVER_NAME,
		.dev_groups = uniwill_groups,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.acpi_match_table = uniwill_acpi_id_table,
		.pm = pm_sleep_ptr(&uniwill_platform_pm_ops),
	},
	.probe = uniwill_platform_probe,
	.remove = uniwill_platform_remove,
	.shutdown = uniwill_platform_shutdown,
};

bool __init uniwill_ec_acpi_interface_available(void)
{
	return acpi_dev_found(UNIWILL_ACPI_HID);
}

int __init uniwill_ec_register_platform_driver(void)
{
	return platform_driver_register(&uniwill_platform_driver);
}

void uniwill_ec_unregister_platform_driver(void)
{
	platform_driver_unregister(&uniwill_platform_driver);
}
