/*
 * cx2092x.h -- CX20921 and CX20924 Audio driver
 *
 * Copyright:   (C) 2017 Conexant Systems, Inc.
 *
 * This is based on Alexander Sverdlin's CS4271 driver code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __CX2092X_PRIV_H__
#define __CX2092X_PRIV_H__

#include <linux/regmap.h>

extern const struct of_device_id cx2092x_dt_ids[];
extern const struct regmap_config cx2092x_regmap_config;

int cx2092x_dev_probe(struct device *dev, struct regmap *regmap);
#if 1 // hjkoh
int cx2092x_dev_remove(struct device *dev);
#endif

#define CX2092X_REG_MAX 0x2000

#endif
