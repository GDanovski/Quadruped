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
 * MEstatusHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "quadruped/quadruped.h"
#include "move_controller/move_controller.h"

LOG_MODULE_REGISTER(move_controller, CONFIG_MOVE_CONTROLLER_MODULE_LOG_LEVEL);

/* Shorthand aliases for readability inside the gait helpers */
#define CU QUADRUPED_LEG_MOVEMENT_COXA_UP
#define CD QUADRUPED_LEG_MOVEMENT_COXA_DOWN
#define FF QUADRUPED_LEG_MOVEMENT_FEMUR_FORWARD
#define FB QUADRUPED_LEG_MOVEMENT_FEMUR_BACKWORD
#define FI QUADRUPED_LEG_MOVEMENT_FEMUR_IDLE

#define FL QUADRUPED_LEG_FRONT_LEFT
#define FR QUADRUPED_LEG_FRONT_RIGHT
#define BL QUADRUPED_LEG_BACK_LEFT
#define BR QUADRUPED_LEG_BACK_RIGHT

#define MOVE_DELAY_SHORT_DIVISOR 24u

enum gait_progression
{
    GAIT_PROGRESSION_FORWARD = 0,
    GAIT_PROGRESSION_REVERSE,
};

static uint32_t gait_short_delay_ms(uint32_t delay_ms)
{
    if (delay_ms == 0u)
    {
        return 0u;
    }

    return MAX(1u, delay_ms / MOVE_DELAY_SHORT_DIVISOR);
}

static uint32_t gait_half_delay_ms(uint32_t delay_ms)
{
    if (delay_ms == 0u)
    {
        return 0u;
    }

    return MAX(1u, delay_ms / 2u);
}

static void gait_sleep(uint32_t delay_ms)
{
    if (delay_ms > 0u)
    {
        k_msleep(delay_ms);
    }
}

static int gait_apply_movement(enum quadruped_leg_index leg,
                               enum quadruped_leg_movement movement)
{
    int ret = quadruped_set_leg_movement(leg, movement);

    if (ret != 0)
    {
        LOG_ERR("movement failed leg=%d movement=%d ret=%d", (int)leg, (int)movement, ret);
    }

    return ret;
}

static int gait_apply_action(enum quadruped_leg_index leg,
                             enum quadruped_leg_movement movement,
                             uint32_t wait_ms)
{
    int ret = gait_apply_movement(leg, movement);

    gait_sleep(wait_ms);

    return ret;
}

static enum gait_progression gait_leg_progression(enum quadruped_leg_index leg,
                                                  enum gait_progression left_progression,
                                                  enum gait_progression right_progression)
{
    switch (leg)
    {
    case FL:
    case BL:
        return left_progression;
    case FR:
    case BR:
        return right_progression;
    default:
        return GAIT_PROGRESSION_FORWARD;
    }
}

static enum quadruped_leg_movement gait_femur_movement_for_leg(enum quadruped_leg_index leg,
                                                               bool lifted,
                                                               enum gait_progression left_progression,
                                                               enum gait_progression right_progression)
{
    enum gait_progression progression = gait_leg_progression(leg, left_progression, right_progression);

    if (lifted)
    {
        return (progression == GAIT_PROGRESSION_FORWARD) ? FF : FB;
    }

    return (progression == GAIT_PROGRESSION_FORWARD) ? FB : FF;
}

static int gait_set_idle_pose(void)
{
    int status = 0;

    for (enum quadruped_leg_index leg = 0; leg < QUADRUPED_LEG_COUNT; leg++)
    {
        if (gait_apply_movement(leg, CD) != 0)
        {
            status = -EIO;
        }

        if (gait_apply_movement(leg, FI) != 0)
        {
            status = -EIO;
        }
    }

    return status;
}

static int gait_run_half_cycle(enum quadruped_leg_index lift_leg_1,
                               enum quadruped_leg_index lift_leg_2,
                               enum quadruped_leg_index support_leg_1,
                               enum quadruped_leg_index support_leg_2,
                               enum gait_progression left_progression,
                               enum gait_progression right_progression,
                               uint32_t delay_ms)
{
    int status = 0;
    const uint32_t short_delay_ms = gait_short_delay_ms(delay_ms);
    const uint32_t half_delay_ms = gait_half_delay_ms(delay_ms);

    if (gait_apply_action(lift_leg_1, CU, short_delay_ms) != 0)
    {
        status = -EIO;
    }

    if (gait_apply_action(lift_leg_2, CU, short_delay_ms) != 0)
    {
        status = -EIO;
    }

    if (gait_apply_action(support_leg_1,
                          gait_femur_movement_for_leg(support_leg_1, false,
                                                      left_progression, right_progression),
                          short_delay_ms) != 0)
    {
        status = -EIO;
    }

    if (gait_apply_action(support_leg_2,
                          gait_femur_movement_for_leg(support_leg_2, false,
                                                      left_progression, right_progression),
                          half_delay_ms) != 0)
    {
        status = -EIO;
    }

    if (gait_apply_action(lift_leg_1,
                          gait_femur_movement_for_leg(lift_leg_1, true,
                                                      left_progression, right_progression),
                          short_delay_ms) != 0)
    {
        status = -EIO;
    }

    if (gait_apply_action(lift_leg_2,
                          gait_femur_movement_for_leg(lift_leg_2, true,
                                                      left_progression, right_progression),
                          delay_ms) != 0)
    {
        status = -EIO;
    }

    if (gait_apply_action(lift_leg_1, CD, short_delay_ms) != 0)
    {
        status = -EIO;
    }

    if (gait_apply_action(lift_leg_2, CD, delay_ms) != 0)
    {
        status = -EIO;
    }

    return status;
}

static int gait_run_cycle(enum gait_progression left_progression,
                          enum gait_progression right_progression,
                          uint32_t delay_ms)
{
    int status = 0;

    if (gait_run_half_cycle(FR, BL, FL, BR, left_progression, right_progression, delay_ms) != 0)
    {
        status = -EIO;
    }

    if (gait_run_half_cycle(FL, BR, FR, BL, left_progression, right_progression, delay_ms) != 0)
    {
        status = -EIO;
    }

    return status;
}

static int move_controller_init(void)
{
    int ret = move_controller_execute(MOVE_COMMAND_IDLE, 0u);

    if (ret != 0)
    {
        LOG_ERR("failed to set idle pose during init: %d", ret);
    }

    return ret;
}

SYS_INIT(move_controller_init, POST_KERNEL, CONFIG_MOVE_CONTROLLER_MODULE_INIT_PRIORITY);

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */
int move_controller_execute(enum move_command cmd, uint32_t delay_ms)
{
    if (cmd >= MOVE_COMMAND_COUNT)
    {
        LOG_ERR("Unknown move command: %d", (int)cmd);
        return -EINVAL;
    }

    LOG_DBG("cmd=%d delay_ms=%u", (int)cmd, delay_ms);

    switch (cmd)
    {
    case MOVE_COMMAND_IDLE:
        return gait_set_idle_pose();
    case MOVE_COMMAND_FORWARD:
        return gait_run_cycle(GAIT_PROGRESSION_FORWARD, GAIT_PROGRESSION_FORWARD, delay_ms);
    case MOVE_COMMAND_REVERSE:
        return gait_run_cycle(GAIT_PROGRESSION_REVERSE, GAIT_PROGRESSION_REVERSE, delay_ms);
    case MOVE_COMMAND_ROTATE_LEFT:
    {
        int status = gait_run_cycle(GAIT_PROGRESSION_FORWARD, GAIT_PROGRESSION_REVERSE, delay_ms);

        if (gait_run_cycle(GAIT_PROGRESSION_FORWARD, GAIT_PROGRESSION_REVERSE, delay_ms) != 0)
        {
            status = -EIO;
        }

        return status;
    }
    case MOVE_COMMAND_ROTATE_RIGHT:
    {
        int status = gait_run_cycle(GAIT_PROGRESSION_REVERSE, GAIT_PROGRESSION_FORWARD, delay_ms);

        if (gait_run_cycle(GAIT_PROGRESSION_REVERSE, GAIT_PROGRESSION_FORWARD, delay_ms) != 0)
        {
            status = -EIO;
        }

        return status;
    }
    default:
        return -EINVAL;
    }
}
