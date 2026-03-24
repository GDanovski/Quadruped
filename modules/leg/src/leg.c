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
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "leg/leg.h"

#define DT_DRV_COMPAT danovski_leg

LOG_MODULE_REGISTER(leg, CONFIG_LOG_DEFAULT_LEVEL);

BUILD_ASSERT(DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT),
             "No status=\"okay\" danovski,leg nodes found in devicetree");

struct leg_config
{
    struct pwm_dt_spec coxa;
    struct pwm_dt_spec femur;
    uint32_t coxa_min_pulse;
    uint32_t coxa_max_pulse;
    uint32_t femur_min_pulse;
    uint32_t femur_max_pulse;
};

struct leg_data
{
    uint8_t reserved;
};

static int leg_init(const struct device *dev)
{
    const struct leg_config *cfg = dev->config;

    if (!device_is_ready(cfg->coxa.dev))
    {
        LOG_ERR("%s: coxa servo is not ready", dev->name);
        return -ENODEV;
    }

    if (!device_is_ready(cfg->femur.dev))
    {
        LOG_ERR("%s: femur servo is not ready", dev->name);
        return -ENODEV;
    }

    if ((cfg->coxa_min_pulse > cfg->coxa_max_pulse) ||
        (cfg->femur_min_pulse > cfg->femur_max_pulse))
    {
        LOG_ERR("%s: invalid pulse limits", dev->name);
        return -EINVAL;
    }

    LOG_INF("%s initialized", dev->name);
    return 0;
}

int leg_set_coxa_pulse_width(const struct device *leg_dev, uint32_t pulse_width_us)
{
    const struct leg_config *cfg;
    uint32_t pulse_width_ns;

    if (leg_dev == NULL)
    {
        return -EINVAL;
    }

    cfg = leg_dev->config;
    pulse_width_ns = PWM_USEC(pulse_width_us);

    if ((pulse_width_ns < cfg->coxa_min_pulse) ||
        (pulse_width_ns > cfg->coxa_max_pulse))
    {
        return -ERANGE;
    }

    return pwm_set_pulse_dt(&cfg->coxa, pulse_width_ns);
}

int leg_set_femur_pulse_width(const struct device *leg_dev, uint32_t pulse_width_us)
{
    const struct leg_config *cfg;
    uint32_t pulse_width_ns;

    if (leg_dev == NULL)
    {
        return -EINVAL;
    }

    cfg = leg_dev->config;
    pulse_width_ns = PWM_USEC(pulse_width_us);

    if ((pulse_width_ns < cfg->femur_min_pulse) ||
        (pulse_width_ns > cfg->femur_max_pulse))
    {
        return -ERANGE;
    }

    return pwm_set_pulse_dt(&cfg->femur, pulse_width_ns);
}

#define LEG_DEFINE(inst)                                            \
    static struct leg_data leg_data_##inst;                         \
    static const struct leg_config leg_cfg_##inst = {               \
        .coxa = PWM_DT_SPEC_GET_BY_NAME(DT_DRV_INST(inst), coxa),   \
        .femur = PWM_DT_SPEC_GET_BY_NAME(DT_DRV_INST(inst), femur), \
        .coxa_min_pulse = DT_INST_PROP(inst, coxa_min_pulse),       \
        .coxa_max_pulse = DT_INST_PROP(inst, coxa_max_pulse),       \
        .femur_min_pulse = DT_INST_PROP(inst, femur_min_pulse),     \
        .femur_max_pulse = DT_INST_PROP(inst, femur_max_pulse),     \
    };                                                              \
    DEVICE_DT_INST_DEFINE(inst, leg_init, NULL, &leg_data_##inst,   \
                          &leg_cfg_##inst, POST_KERNEL,             \
                          CONFIG_LEG_MODULE_INIT_PRIORITY, NULL)

DT_INST_FOREACH_STATUS_OKAY(LEG_DEFINE)
