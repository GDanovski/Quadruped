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
#include "quadruped/quadruped.h"

#define DT_DRV_COMPAT danovski_quadruped

LOG_MODULE_REGISTER(quadruped, CONFIG_QUADRUPED_MODULE_LOG_LEVEL);

BUILD_ASSERT(DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT),
             "No status=\"okay\" danovski,quadruped nodes found in devicetree");

/* [leg][movement] Map for all legs and movement commands. */
static const uint16_t
    quadruped_leg_movement_matrix[QUADRUPED_LEG_COUNT]
                                 [QUADRUPED_LEG_MOVEMENT_COUNT] = {
                                     [QUADRUPED_LEG_FRONT_LEFT] = {
                                         [QUADRUPED_LEG_MOVEMENT_COXA_UP] = 800u,
                                         [QUADRUPED_LEG_MOVEMENT_COXA_DOWN] = 500u,
                                         [QUADRUPED_LEG_MOVEMENT_FEMUR_FORWARD] = 1750u,
                                         [QUADRUPED_LEG_MOVEMENT_FEMUR_BACKWORD] = 1150u,
                                         [QUADRUPED_LEG_MOVEMENT_FEMUR_IDLE] = 1450u,
                                     },
                                     [QUADRUPED_LEG_FRONT_RIGHT] = {
                                         [QUADRUPED_LEG_MOVEMENT_COXA_UP] = 800u,
                                         [QUADRUPED_LEG_MOVEMENT_COXA_DOWN] = 500u,
                                         [QUADRUPED_LEG_MOVEMENT_FEMUR_FORWARD] = 1150u,
                                         [QUADRUPED_LEG_MOVEMENT_FEMUR_BACKWORD] = 1750u,
                                         [QUADRUPED_LEG_MOVEMENT_FEMUR_IDLE] = 1450u,
                                     },
                                     [QUADRUPED_LEG_BACK_LEFT] = {
                                         [QUADRUPED_LEG_MOVEMENT_COXA_UP] = 800u,
                                         [QUADRUPED_LEG_MOVEMENT_COXA_DOWN] = 500u,
                                         [QUADRUPED_LEG_MOVEMENT_FEMUR_FORWARD] = 1750u,
                                         [QUADRUPED_LEG_MOVEMENT_FEMUR_BACKWORD] = 1150u,
                                         [QUADRUPED_LEG_MOVEMENT_FEMUR_IDLE] = 1450u,
                                     },
                                     [QUADRUPED_LEG_BACK_RIGHT] = {
                                         [QUADRUPED_LEG_MOVEMENT_COXA_UP] = 800u,
                                         [QUADRUPED_LEG_MOVEMENT_COXA_DOWN] = 500u,
                                         [QUADRUPED_LEG_MOVEMENT_FEMUR_FORWARD] = 1150u,
                                         [QUADRUPED_LEG_MOVEMENT_FEMUR_BACKWORD] = 1750u,
                                         [QUADRUPED_LEG_MOVEMENT_FEMUR_IDLE] = 1450u,
                                     },
};

/** @brief Quadruped device data structure. */
struct quadruped_data
{
    const struct device *leg_devs[QUADRUPED_LEG_COUNT]; /**< Leg device pointers. */
};

int quadruped_set_leg_movement(enum quadruped_leg_index leg_index,
                               enum quadruped_leg_movement movement)
{
    const struct device *quadruped_dev = DEVICE_DT_GET(DT_NODELABEL(quadruped));
    struct quadruped_data *data;
    const struct device *leg_dev;
    int err;

    if (!device_is_ready(quadruped_dev))
    {
        LOG_ERR("quadruped device is not ready");
        return -ENODEV;
    }

    if (leg_index >= QUADRUPED_LEG_COUNT)
    {
        LOG_ERR("invalid leg index: %d", (int)leg_index);
        return -EINVAL;
    }

    if (movement >= QUADRUPED_LEG_MOVEMENT_COUNT)
    {
        LOG_ERR("invalid leg movement: %d", (int)movement);
        return -EINVAL;
    }

    data = quadruped_dev->data;
    leg_dev = data->leg_devs[leg_index];

    uint32_t val = quadruped_leg_movement_matrix[leg_index][movement];

    switch (movement)
    {
    case QUADRUPED_LEG_MOVEMENT_COXA_UP:
    case QUADRUPED_LEG_MOVEMENT_COXA_DOWN:
        err = leg_set_coxa_pulse_width(leg_dev, val);
        break;
    case QUADRUPED_LEG_MOVEMENT_FEMUR_FORWARD:
    case QUADRUPED_LEG_MOVEMENT_FEMUR_BACKWORD:
    case QUADRUPED_LEG_MOVEMENT_FEMUR_IDLE:
        err = leg_set_femur_pulse_width(leg_dev, val);
        break;
    default:
        LOG_ERR("invalid leg movement: %d", (int)movement);
        return -EINVAL;
    }

    if (err != 0)
    {
        LOG_ERR("leg %u set failed: %d", leg_index, err);
        return err;
    }

    LOG_DBG("leg %u movement %u set to %u us", leg_index, movement, val);

    return 0;
}

static int quadruped_device_init(const struct device *dev)
{
    struct quadruped_data *data = dev->data;
    int i;

    /* Retrieve legs from device tree properties. */
    data->leg_devs[QUADRUPED_LEG_FRONT_LEFT] =
        DEVICE_DT_GET(DT_PHANDLE(DT_NODELABEL(quadruped), leg_front_left));
    data->leg_devs[QUADRUPED_LEG_FRONT_RIGHT] =
        DEVICE_DT_GET(DT_PHANDLE(DT_NODELABEL(quadruped), leg_front_right));
    data->leg_devs[QUADRUPED_LEG_BACK_LEFT] =
        DEVICE_DT_GET(DT_PHANDLE(DT_NODELABEL(quadruped), leg_back_left));
    data->leg_devs[QUADRUPED_LEG_BACK_RIGHT] =
        DEVICE_DT_GET(DT_PHANDLE(DT_NODELABEL(quadruped), leg_back_right));

    /* Check readiness of each leg device and its servos. */
    for (i = 0; i < QUADRUPED_LEG_COUNT; i++)
    {
        if (!device_is_ready(data->leg_devs[i]))
        {
            LOG_ERR("%s: leg %d device is not ready", dev->name, i);
            return -ENODEV;
        }
    }

    LOG_DBG("quadruped device initialized");
    return 0;
}

static struct quadruped_data quadruped_data_0;

DEVICE_DT_DEFINE(DT_NODELABEL(quadruped),
                 quadruped_device_init, NULL,
                 &quadruped_data_0, NULL,
                 POST_KERNEL, CONFIG_QUADRUPED_MODULE_INIT_PRIORITY, NULL);
