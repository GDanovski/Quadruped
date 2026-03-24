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

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Return the coxa servo device used by a LEG instance.
     *
     * @param leg_dev LEG MFD device.
     * @return Servo device pointer on success, NULL on invalid input.
     */
    const struct device *leg_get_coxa(const struct device *leg_dev);

    /**
     * @brief Return the femur servo device used by a LEG instance.
     *
     * @param leg_dev LEG MFD device.
     * @return Servo device pointer on success, NULL on invalid input.
     */
    const struct device *leg_get_femur(const struct device *leg_dev);

#ifdef __cplusplus
}
#endif

#endif /* LEG_LEG_H_ */
