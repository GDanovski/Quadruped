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

#ifndef MOVE_CONTROLLER_MOVE_CONTROLLER_H_
#define MOVE_CONTROLLER_MOVE_CONTROLLER_H_

#include <errno.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief High-level move commands accepted by the move controller. */
    enum move_command
    {
        MOVE_COMMAND_IDLE = 0,     /**< Stop and hold neutral stance. */
        MOVE_COMMAND_FORWARD,      /**< Advance one gait step forward. */
        MOVE_COMMAND_REVERSE,      /**< Advance one gait step backward. */
        MOVE_COMMAND_ROTATE_LEFT,  /**< Advance one gait step rotating left. */
        MOVE_COMMAND_ROTATE_RIGHT, /**< Advance one gait step rotating right. */
    };

/**
 * @brief Execute one step of the gait sequence for the given command.
 *
 * Repeated calls with the same command cycle through the gait phases.
 * Switching to a different command resets the phase counter so the new
 * gait starts cleanly from phase 0.
 *
 * @param cmd Move command to execute.
 * @return 0 on success, negative errno otherwise.
 */
#if defined(CONFIG_MOVE_CONTROLLER_MODULE)
    int move_controller_execute(enum move_command cmd);
#else
static inline int move_controller_execute(enum move_command cmd)
{
    ARG_UNUSED(cmd);
    return -ENOTSUP;
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* MOVE_CONTROLLER_MOVE_CONTROLLER_H_ */
