/*
 * Copyright (C) 2026 Georgi Danovski
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <errno.h>

#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "leg/leg.h"

#define DT_DRV_COMPAT zephyr_leg

LOG_MODULE_REGISTER(leg, CONFIG_LOG_DEFAULT_LEVEL);

BUILD_ASSERT(DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT),
             "No status=\"okay\" zephyr,leg nodes found in devicetree");

struct leg_config
{
    const struct device *coxa;
    const struct device *femur;
};

struct leg_data
{
    uint8_t reserved;
};

static int leg_init(const struct device *dev)
{
    const struct leg_config *cfg = dev->config;

    if (!device_is_ready(cfg->coxa))
    {
        LOG_ERR("%s: coxa servo is not ready", dev->name);
        return -ENODEV;
    }

    if (!device_is_ready(cfg->femur))
    {
        LOG_ERR("%s: femur servo is not ready", dev->name);
        return -ENODEV;
    }

    LOG_INF("%s initialized", dev->name);
    return 0;
}

const struct device *leg_get_coxa(const struct device *leg_dev)
{
    const struct leg_config *cfg;

    if (leg_dev == NULL)
    {
        return NULL;
    }

    cfg = leg_dev->config;
    return cfg->coxa;
}

const struct device *leg_get_femur(const struct device *leg_dev)
{
    const struct leg_config *cfg;

    if (leg_dev == NULL)
    {
        return NULL;
    }

    cfg = leg_dev->config;
    return cfg->femur;
}

#define LEG_DEFINE(inst)                                          \
    static struct leg_data leg_data_##inst;                       \
    static const struct leg_config leg_cfg_##inst = {             \
        .coxa = DEVICE_DT_GET(DT_INST_PHANDLE(inst, coxa)),       \
        .femur = DEVICE_DT_GET(DT_INST_PHANDLE(inst, femur)),     \
    };                                                            \
    DEVICE_DT_INST_DEFINE(inst, leg_init, NULL, &leg_data_##inst, \
                          &leg_cfg_##inst, POST_KERNEL,           \
                          CONFIG_KERNEL_INIT_PRIORITY_DEVICE, NULL)

DT_INST_FOREACH_STATUS_OKAY(LEG_DEFINE)
