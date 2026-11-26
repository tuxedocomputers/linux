// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * WMI backend for accessing the Uniwill embedded controller.
 *
 * Special thanks go to Pőcze Barnabás, Christoffer Sandberg and Werner Sembach
 * for supporting the development of this driver either through prior work or
 * by answering questions regarding the underlying ACPI and WMI interfaces.
 *
 * Copyright (C) 2026 Armin Wolf <W_Armin@gmx.de>
 */

#include <linux/cleanup.h>
#include <linux/compiler_attributes.h>
#include <linux/device.h>
#include <linux/device/driver.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/limits.h>
#include <linux/pm.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/wmi.h>

#include "wmi-ec.h"

#include "core.h"

#define UNIWILL_WMI_GUID	"ABBC0F6F-8EA1-11D1-00A0-C90629100000"

#define WMI_ERROR_MAGIC		0xFEFEFEFE

enum uniwill_wmi_method {
	UNIWILL_GET_ULONG	= 0x01,
	UNIWILL_SET_ULONG	= 0x02,
	UNIWILL_FIRE_ULONG	= 0x03,
	UNIWILL_GET_SET_ULONG	= 0x04,
	UNIWILL_GET_BUTTON	= 0x05,
};

struct uniwill_wmi_method_buffer {
	__le16 address;
	__le16 data;
	__le16 operation;
	__le16 reserved;
} __packed;

static int uniwill_wmi_get_set_ulong(struct wmi_device *wdev,
				     struct uniwill_wmi_method_buffer *input, u32 *output)
{
	struct wmi_buffer in = {
		.length = sizeof(*input),
		.data = input,
	};
	struct wmi_buffer out;
	int ret;

	ret = wmidev_invoke_method(wdev, 0x0, UNIWILL_GET_SET_ULONG, &in, &out,
				   sizeof(__le32));
	if (ret < 0)
		return ret;

	__le32 *result __free(kfree) = out.data;

	*output = le32_to_cpu(*result);

	return 0;
}

static int uniwill_wmi_ec_reg_write(void *context, unsigned int reg, unsigned int val)
{
	struct uniwill_wmi_method_buffer input = {
		.address = cpu_to_le16(reg),
		.data = cpu_to_le16(val & U8_MAX),
		.operation = 0x0000,
	};
	struct wmi_device *wdev = context;
	u32 output;
	int ret;

	ret = uniwill_wmi_get_set_ulong(wdev, &input, &output);
	if (ret < 0)
		return ret;

	if (output == WMI_ERROR_MAGIC)
		return -ENXIO;

	return 0;
}

static int uniwill_wmi_ec_reg_read(void *context, unsigned int reg, unsigned int *val)
{
	struct uniwill_wmi_method_buffer input = {
		.address = cpu_to_le16(reg),
		.data = 0x0000,
		.operation = cpu_to_le16(0x0100),
	};
	struct wmi_device *wdev = context;
	u32 output;
	int ret;

	ret = uniwill_wmi_get_set_ulong(wdev, &input, &output);
	if (ret < 0)
		return ret;

	if (output == WMI_ERROR_MAGIC)
		return -ENXIO;

	*val = (u8)output;

	return 0;
}

int uniwill_wmi_call_cmd(struct wmi_device *wdev, u8 cmd)
{
	struct uniwill_wmi_method_buffer input = {
		.address = cpu_to_le16(cmd),
		.operation = cpu_to_le16(0x0500),
	};
	u32 output;
	int ret;

	ret = uniwill_wmi_get_set_ulong(wdev, &input, &output);
	if (ret < 0)
		return ret;

	if (output == WMI_ERROR_MAGIC)
		return -ENXIO;

	return 0;
}

static const struct regmap_bus uniwill_wmi_ec_bus = {
	.reg_write = uniwill_wmi_ec_reg_write,
	.reg_read = uniwill_wmi_ec_reg_read,
	.reg_format_endian_default = REGMAP_ENDIAN_LITTLE,
	.val_format_endian_default = REGMAP_ENDIAN_LITTLE,
};

static int uniwill_wmi_probe(struct wmi_device *wdev, const void *context)
{
	return uniwill_probe(&wdev->dev, &uniwill_wmi_ec_bus, wdev, UNIWILL_WMI_EC);
}

static void uniwill_wmi_remove(struct wmi_device *wdev)
{
	uniwill_remove(&wdev->dev, UNIWILL_WMI_EC);
}

static void uniwill_wmi_shutdown(struct wmi_device *wdev)
{
	uniwill_shutdown(&wdev->dev);
}

static DEFINE_SIMPLE_DEV_PM_OPS(uniwill_wmi_pm_ops, uniwill_suspend, uniwill_resume);

/*
 * We cannot fully trust this GUID since Uniwill just copied the WMI GUID
 * from the Windows driver example, and others probably did the same.
 *
 * Because of this we cannot use this WMI GUID for autoloading.
 */
static const struct wmi_device_id uniwill_wmi_id_table[] = {
	{ UNIWILL_WMI_GUID, NULL },
	{ }
};

static struct wmi_driver uniwill_wmi_driver = {
	.driver = {
		.name = UNIWILL_EC_DRIVER_NAME,
		.dev_groups = uniwill_groups,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.pm = pm_sleep_ptr(&uniwill_wmi_pm_ops),
	},
	.id_table = uniwill_wmi_id_table,
	.probe = uniwill_wmi_probe,
	.remove = uniwill_wmi_remove,
	.shutdown = uniwill_wmi_shutdown,
	.no_singleton = true,
};

int __init uniwill_ec_register_wmi_driver(void)
{
	return wmi_driver_register(&uniwill_wmi_driver);
}

void uniwill_ec_unregister_wmi_driver(void)
{
	wmi_driver_unregister(&uniwill_wmi_driver);
}
