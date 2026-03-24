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

#ifndef QUADRUPED_QUADRUPED_H_
#define QUADRUPED_QUADRUPED_H_

#include <errno.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief Leg selector values used by the quadruped controller. */
    enum quadruped_leg_index
    {
        QUADRUPED_LEG_FRONT_LEFT = 0, /**< Front-left leg device index. */
        QUADRUPED_LEG_FRONT_RIGHT,    /**< Front-right leg device index. */
        QUADRUPED_LEG_BACK_LEFT,      /**< Back-left leg device index. */
        QUADRUPED_LEG_BACK_RIGHT,     /**< Back-right leg device index. */
        QUADRUPED_LEG_COUNT,          /**< Total number of managed legs. */
    };

    /** @brief Supported leg movement commands. */
    enum quadruped_leg_movement
    {
        QUADRUPED_LEG_MOVEMENT_COXA_UP = 0,    /**< Move coxa up. */
        QUADRUPED_LEG_MOVEMENT_COXA_DOWN,      /**< Move coxa down. */
        QUADRUPED_LEG_MOVEMENT_FEMUR_FORWARD,  /**< Move femur forward. */
        QUADRUPED_LEG_MOVEMENT_FEMUR_BACKWORD, /**< Move femur backword. */
        QUADRUPED_LEG_MOVEMENT_FEMUR_IDLE,     /**< Keep femur idle. */
        QUADRUPED_LEG_MOVEMENT_COUNT,          /**< Total number of leg movements. */
    };

/**
 * @brief Set one leg to a predefined movement state.
 *
 * @param leg_index Target leg index.
 * @param movement Movement command to apply.
 * @return 0 on success, negative errno otherwise.
 */
#if defined(CONFIG_QUADRUPED_MODULE)
    int quadruped_set_leg_movement(enum quadruped_leg_index leg_index,
                                   enum quadruped_leg_movement movement);
#else
static inline int quadruped_set_leg_movement(enum quadruped_leg_index leg_index,
                                             enum quadruped_leg_movement movement)
{
    ARG_UNUSED(leg_index);
    ARG_UNUSED(movement);
    return -ENOTSUP;
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* QUADRUPED_QUADRUPED_H_ */
