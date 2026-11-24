/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Linux driver for Uniwill notebooks.
 *
 * Copyright (C) 2026 Armin Wolf <W_Armin@gmx.de>
 */

#ifndef UNIWILL_CORE_H
#define UNIWILL_CORE_H

/* For backwards compatibility */
#define UNIWILL_EC_DRIVER_NAME	"uniwill"

extern const struct attribute_group *const uniwill_groups[];

int uniwill_probe(struct device *dev, const struct regmap_bus *bus, void *bus_context);

int uniwill_suspend(struct device *dev);

int uniwill_resume(struct device *dev);

void uniwill_shutdown(struct device *dev);

#endif	/* UNIWILL_CORE_H */
