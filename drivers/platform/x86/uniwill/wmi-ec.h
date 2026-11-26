/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Linux driver for Uniwill notebooks.
 *
 * Copyright (C) 2026 Armin Wolf <W_Armin@gmx.de>
 */

#ifndef UNIWILL_WMI_EC_H
#define UNIWILL_WMI_EC_H

#define UNIWILL_WMI_CMD_LOCAL_DIMMING_ON	0x0E
#define UNIWILL_WMI_CMD_LOCAL_DIMMING_OFF	0x0D

int uniwill_ec_register_wmi_driver(void) __init;

void uniwill_ec_unregister_wmi_driver(void);

int uniwill_wmi_call_cmd(struct wmi_device *dev, u8 cmd);

#endif	/* UNIWILL_WMI_EC_H */
