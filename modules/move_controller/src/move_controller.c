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
#include <stdint.h>

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "quadruped/quadruped.h"
#include "move_controller/move_controller.h"

LOG_MODULE_REGISTER(move_controller, CONFIG_MOVE_CONTROLLER_MODULE_LOG_LEVEL);

/* -------------------------------------------------------------------------
 * Gait table types
 * -------------------------------------------------------------------------
 * Each gait_leg_state holds one coxa and one femur command for a single leg.
 * A gait_phase groups the states for all four legs in one time-step.
 * A gait_sequence bundles a pointer to an array of phases with its length.
 * ------------------------------------------------------------------------- */

struct gait_leg_state
{
    enum quadruped_leg_movement coxa;
    enum quadruped_leg_movement femur;
};

struct gait_phase
{
    struct gait_leg_state legs[QUADRUPED_LEG_COUNT];
};

struct gait_sequence
{
    const struct gait_phase *phases;
    uint8_t num_phases;
};

/* Helper macro: compact leg-state literal */
#define LS(c, f) {.coxa = (c), .femur = (f)}

/* Shorthand aliases for readability inside the tables */
#define CU QUADRUPED_LEG_MOVEMENT_COXA_UP
#define CD QUADRUPED_LEG_MOVEMENT_COXA_DOWN
#define FF QUADRUPED_LEG_MOVEMENT_FEMUR_FORWARD
#define FB QUADRUPED_LEG_MOVEMENT_FEMUR_BACKWORD
#define FI QUADRUPED_LEG_MOVEMENT_FEMUR_IDLE

#define FL QUADRUPED_LEG_FRONT_LEFT
#define FR QUADRUPED_LEG_FRONT_RIGHT
#define BL QUADRUPED_LEG_BACK_LEFT
#define BR QUADRUPED_LEG_BACK_RIGHT

/* -------------------------------------------------------------------------
 * IDLE — hold neutral stance (single phase, always resolves immediately)
 * ------------------------------------------------------------------------- */
static const struct gait_phase idle_phases[] = {
    {.legs = {[FL] = LS(CD, FI), [FR] = LS(CD, FI), [BL] = LS(CD, FI), [BR] = LS(CD, FI)}},
};

/* -------------------------------------------------------------------------
 * FORWARD — diagonal trot (FL+BR / FR+BL pairs alternate)
 *
 * Phase 0: Lift diagonal pair 1 (FL+BR) and swing forward;
 *          pair 2 (FR+BL) remains grounded and pushes backward.
 * Phase 1: Lower all legs to neutral (weight transfer).
 * Phase 2: Lift diagonal pair 2 (FR+BL) and swing forward;
 *          pair 1 (FL+BR) remains grounded and pushes backward.
 * Phase 3: Lower all legs to neutral (weight transfer).
 * ------------------------------------------------------------------------- */
static const struct gait_phase forward_phases[] = {
    {.legs = {[FL] = LS(CU, FF), [FR] = LS(CD, FB), [BL] = LS(CD, FB), [BR] = LS(CU, FF)}},
    {.legs = {[FL] = LS(CD, FI), [FR] = LS(CD, FI), [BL] = LS(CD, FI), [BR] = LS(CD, FI)}},
    {.legs = {[FL] = LS(CD, FB), [FR] = LS(CU, FF), [BL] = LS(CU, FF), [BR] = LS(CD, FB)}},
    {.legs = {[FL] = LS(CD, FI), [FR] = LS(CD, FI), [BL] = LS(CD, FI), [BR] = LS(CD, FI)}},
};

/* -------------------------------------------------------------------------
 * REVERSE — diagonal trot in the opposite direction
 * ------------------------------------------------------------------------- */
static const struct gait_phase reverse_phases[] = {
    {.legs = {[FL] = LS(CU, FB), [FR] = LS(CD, FF), [BL] = LS(CD, FF), [BR] = LS(CU, FB)}},
    {.legs = {[FL] = LS(CD, FI), [FR] = LS(CD, FI), [BL] = LS(CD, FI), [BR] = LS(CD, FI)}},
    {.legs = {[FL] = LS(CD, FF), [FR] = LS(CU, FB), [BL] = LS(CU, FB), [BR] = LS(CD, FF)}},
    {.legs = {[FL] = LS(CD, FI), [FR] = LS(CD, FI), [BL] = LS(CD, FI), [BR] = LS(CD, FI)}},
};

/* -------------------------------------------------------------------------
 * ROTATE LEFT (counter-clockwise viewed from above)
 *
 * Phase 0: Lift left-side legs (FL+BL) and swing them backward;
 *          right-side legs (FR+BR) stay grounded and push forward.
 * Phase 1: Lower all to neutral.
 * Phase 2: Lift right-side legs (FR+BR) and swing them backward;
 *          left-side legs (FL+BL) stay grounded and push forward.
 * Phase 3: Lower all to neutral.
 * ------------------------------------------------------------------------- */
static const struct gait_phase rotate_left_phases[] = {
    {.legs = {[FL] = LS(CU, FB), [FR] = LS(CD, FF), [BL] = LS(CU, FB), [BR] = LS(CD, FF)}},
    {.legs = {[FL] = LS(CD, FI), [FR] = LS(CD, FI), [BL] = LS(CD, FI), [BR] = LS(CD, FI)}},
    {.legs = {[FL] = LS(CD, FF), [FR] = LS(CU, FB), [BL] = LS(CD, FF), [BR] = LS(CU, FB)}},
    {.legs = {[FL] = LS(CD, FI), [FR] = LS(CD, FI), [BL] = LS(CD, FI), [BR] = LS(CD, FI)}},
};

/* -------------------------------------------------------------------------
 * ROTATE RIGHT (clockwise viewed from above) — mirror of ROTATE LEFT
 * ------------------------------------------------------------------------- */
static const struct gait_phase rotate_right_phases[] = {
    {.legs = {[FL] = LS(CD, FF), [FR] = LS(CU, FB), [BL] = LS(CD, FF), [BR] = LS(CU, FB)}},
    {.legs = {[FL] = LS(CD, FI), [FR] = LS(CD, FI), [BL] = LS(CD, FI), [BR] = LS(CD, FI)}},
    {.legs = {[FL] = LS(CU, FB), [FR] = LS(CD, FF), [BL] = LS(CU, FB), [BR] = LS(CD, FF)}},
    {.legs = {[FL] = LS(CD, FI), [FR] = LS(CD, FI), [BL] = LS(CD, FI), [BR] = LS(CD, FI)}},
};

/* -------------------------------------------------------------------------
 * Gait dispatch table — indexed by enum move_command
 * ------------------------------------------------------------------------- */
static const struct gait_sequence gait_table[] = {
    [MOVE_COMMAND_IDLE] = {idle_phases, ARRAY_SIZE(idle_phases)},
    [MOVE_COMMAND_FORWARD] = {forward_phases, ARRAY_SIZE(forward_phases)},
    [MOVE_COMMAND_REVERSE] = {reverse_phases, ARRAY_SIZE(reverse_phases)},
    [MOVE_COMMAND_ROTATE_LEFT] = {rotate_left_phases, ARRAY_SIZE(rotate_left_phases)},
    [MOVE_COMMAND_ROTATE_RIGHT] = {rotate_right_phases, ARRAY_SIZE(rotate_right_phases)},
};

/* -------------------------------------------------------------------------
 * State
 * ------------------------------------------------------------------------- */
static enum move_command last_command = MOVE_COMMAND_IDLE;
static uint8_t gait_step = 0;

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

    if (cmd != last_command)
    {
        LOG_DBG("Command changed %d -> %d, resetting gait phase", (int)last_command, (int)cmd);
        last_command = cmd;
        gait_step = 0;
    }

    const struct gait_sequence *seq = &gait_table[cmd];
    const struct gait_phase *phase = &seq->phases[gait_step];

    LOG_DBG("cmd=%d phase=%u/%u", (int)cmd, gait_step, seq->num_phases);

    int status = 0;

    for (enum quadruped_leg_index leg = 0; leg < QUADRUPED_LEG_COUNT; leg++)
    {
        int ret;

        ret = quadruped_set_leg_movement(leg, phase->legs[leg].coxa);
        if (ret != 0)
        {
            LOG_ERR("coxa failed leg=%d ret=%d", (int)leg, ret);
            status = ret;
        }

        if (delay_ms > 0u)
        {
            k_msleep(delay_ms);
        }

        ret = quadruped_set_leg_movement(leg, phase->legs[leg].femur);
        if (ret != 0)
        {
            LOG_ERR("femur failed leg=%d ret=%d", (int)leg, ret);
            status = ret;
        }

        if (delay_ms > 0u)
        {
            k_msleep(delay_ms);
        }
    }

    gait_step = (gait_step + 1) % seq->num_phases;

    return status;
}
