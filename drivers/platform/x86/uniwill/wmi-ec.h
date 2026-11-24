/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Linux driver for Uniwill notebooks.
 *
 * Copyright (C) 2026 Armin Wolf <W_Armin@gmx.de>
 */

#ifndef UNIWILL_WMI_EC_H
#define UNIWILL_WMI_EC_H

int uniwill_ec_register_wmi_driver(void) __init;

void uniwill_ec_unregister_wmi_driver(void);

#endif	/* UNIWILL_WMI_EC_H */
