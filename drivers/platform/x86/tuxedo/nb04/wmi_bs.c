// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * This driver implements the WMI BS device found on TUXEDO notebooks with board
 * vendor NB04.
 *
 * Copyright (C) 2025 Werner Sembach <wse@tuxedocomputers.com>
 */

#include <linux/dmi.h>
#include <linux/module.h>
#include <linux/wmi.h>

#include "wmi_util.h"

static const struct wmi_device_id tuxedo_nb04_wmi_bs_device_ids[] = {
	{ .guid_string = "1F174999-3A4E-4311-900D-7BE7166D5055" },
	{ }
};
MODULE_DEVICE_TABLE(wmi, tuxedo_nb04_wmi_bs_device_ids);

static int tux_probe(struct wmi_device *wdev, const void *context __always_unused)
{
	return 0;
}

static struct wmi_driver tuxedo_nb04_wmi_bs_driver = {
	.driver = {
		.name = "tuxedo_nb04_wmi_bs",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
	.id_table = tuxedo_nb04_wmi_bs_device_ids,
	.probe = tux_probe,
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

static int __init tuxedo_nb04_wmi_ab_init(void)
{
	if (!dmi_check_system(tux_tested_devices_dmi_table))
		return -ENODEV;

	return wmi_driver_register(&tuxedo_nb04_wmi_bs_driver);
}
module_init(tuxedo_nb04_wmi_ab_init);

static void __exit tuxedo_nb04_wmi_ab_exit(void)
{
	return wmi_driver_unregister(&tuxedo_nb04_wmi_bs_driver);
}
module_exit(tuxedo_nb04_wmi_ab_exit);

MODULE_DESCRIPTION("Power profile control for TUXEDO NB04 devices");
MODULE_AUTHOR("Werner Sembach <wse@tuxedocomputers.com>");
MODULE_LICENSE("GPL");
