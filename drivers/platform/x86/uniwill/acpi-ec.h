/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Linux driver for Uniwill notebooks.
 *
 * Copyright (C) 2026 Armin Wolf <W_Armin@gmx.de>
 */

#ifndef UNIWILL_ACPI_EC_H
#define UNIWILL_ACPI_EC_H

bool uniwill_ec_acpi_interface_available(void) __init;

int uniwill_ec_register_platform_driver(void) __init;

void uniwill_ec_unregister_platform_driver(void);

#endif	/* UNIWILL_ACPI_EC_H */
