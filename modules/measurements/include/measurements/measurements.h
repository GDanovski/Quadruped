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

#ifndef MEASUREMENTS_MEASUREMENTS_H_
#define MEASUREMENTS_MEASUREMENTS_H_

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Get the measurements sensor device.
 *
 * @return Pointer to the sensor device, or NULL if unavailable.
 */
#if defined(CONFIG_MEASUREMENTS_MODULE)
    const struct device *measurements_get_device(void);
    int measurements_sample_fetch(void);
    int measurements_channel_get(struct sensor_value *val);
#else
static inline const struct device *measurements_get_device(void)
{
    return NULL;
}

static inline int measurements_sample_fetch(void)
{
    return -ENOTSUP;
}

static inline int measurements_channel_get(struct sensor_value *val)
{
    ARG_UNUSED(val);
    return -ENOTSUP;
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* MEASUREMENTS_MEASUREMENTS_H_ */
