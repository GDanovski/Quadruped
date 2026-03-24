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

#ifndef LEG_LEG_H_
#define LEG_LEG_H_

#include <errno.h>
#include <stdint.h>

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Set coxa pulse width for a LEG instance.
 *
 * @param leg_dev LEG MFD device.
 * @param pulse_width_us Pulse width in microseconds.
 * @return 0 on success, negative errno otherwise.
 */
#if defined(CONFIG_LEG_MODULE)
    int leg_set_coxa_pulse_width(const struct device *leg_dev,
                                 uint32_t pulse_width_us);
#else
static inline int leg_set_coxa_pulse_width(const struct device *leg_dev,
                                           uint32_t pulse_width_us)
{
    ARG_UNUSED(leg_dev);
    ARG_UNUSED(pulse_width_us);
    return -ENOTSUP;
}
#endif

/**
 * @brief Set femur pulse width for a LEG instance.
 *
 * @param leg_dev LEG MFD device.
 * @param pulse_width_us Pulse width in microseconds.
 * @return 0 on success, negative errno otherwise.
 */
#if defined(CONFIG_LEG_MODULE)
    int leg_set_femur_pulse_width(const struct device *leg_dev,
                                  uint32_t pulse_width_us);
#else
static inline int leg_set_femur_pulse_width(const struct device *leg_dev,
                                            uint32_t pulse_width_us)
{
    ARG_UNUSED(leg_dev);
    ARG_UNUSED(pulse_width_us);
    return -ENOTSUP;
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* LEG_LEG_H_ */
