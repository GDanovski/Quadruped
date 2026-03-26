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

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include "ble_peripheral/ble_peripheral.h"
#include "measurements/measurements.h"
#include "move_controller/move_controller.h"
#include "voltage_regulator/voltage_regulator.h"

#define APP_MOVE_DELAY_MS 500u
#define APP_MEASUREMENTS_SAMPLE_PERIOD_MS 1000u
#define APP_MEASUREMENTS_THREAD_STACK_SIZE 1024
#define APP_MEASUREMENTS_THREAD_PRIORITY 6
#define APP_BLE_THREAD_STACK_SIZE 1024
#define APP_BLE_THREAD_PRIORITY 7
#define APP_BLE_NOTIFY_PERIOD_MS 1000u
#define APP_MOVE_THREAD_STACK_SIZE 1024
#define APP_MOVE_THREAD_PRIORITY 5

LOG_MODULE_REGISTER(app, CONFIG_APP_LOG_LEVEL);

/* Shared move command state: producer threads update it, movement thread consumes it. */
atomic_t app_move_command = ATOMIC_INIT(MOVE_COMMAND_IDLE);
atomic_t app_voltage_mv = ATOMIC_INIT(0);

static void app_ble_movement_cb(enum move_command movement)
{
    atomic_set(&app_move_command, (atomic_val_t)movement);
}

static int32_t app_ble_voltage_mv_cb(void)
{
    return (int32_t)atomic_get(&app_voltage_mv);
}

static void app_ble_disconnected_cb(void)
{
    LOG_DBG("BLE disconnected, resetting move command to IDLE");
    atomic_set(&app_move_command, (atomic_val_t)MOVE_COMMAND_IDLE);
}

static void app_move_thread(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    while (1)
    {
        enum move_command cmd = (enum move_command)atomic_get(&app_move_command);

        if (cmd >= MOVE_COMMAND_COUNT)
        {
            LOG_ERR("Invalid move command in app_move_command: %d", (int)cmd);
            cmd = MOVE_COMMAND_IDLE;
        }

        int ret = move_controller_execute(cmd, APP_MOVE_DELAY_MS);

        if (ret != 0)
        {
            LOG_ERR("move_controller failed: %d", ret);
        }
    }
}

static void app_measurements_thread(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    uint32_t avg_mv = 0;
    uint32_t sample_sum = 0;
    const int sample_count = 100;
    bool sample_init = false;

    while (1)
    {
        int ret = measurements_sample_fetch();
        if (ret != 0)
        {
            LOG_ERR("measurements_sample_fetch failed: %d", ret);
            continue;
        }

        struct sensor_value val;
        ret = measurements_channel_get(&val);
        if (ret != 0)
        {
            LOG_ERR("measurements_channel_get failed: %d", ret);
            continue;
        }

        int32_t sample_mv = (val.val1 * 1000) + (val.val2 / 1000);

        if (sample_mv < 0)
        {
            LOG_ERR("Invalid measurement value: %d mV", sample_mv);
            continue;
        }

        /* Initialize moving average with the first sample, then update with a rolling sum. */
        if (!sample_init)
        {
            avg_mv = sample_mv;
            sample_sum = avg_mv * sample_count;
            sample_init = true;
        }
        else
        {
            sample_sum -= avg_mv;
            sample_sum += sample_mv;
            avg_mv = sample_sum / sample_count;
        }

        atomic_set(&app_voltage_mv, (atomic_val_t)avg_mv);
        LOG_DBG("measurements: %d mV (avg: %u mV)", sample_mv, avg_mv);

        k_sleep(K_MSEC(APP_MEASUREMENTS_SAMPLE_PERIOD_MS));
    }
}

static void app_ble_thread(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    struct ble_peripheral_cb ble_callbacks = {
        .movement_cb = app_ble_movement_cb,
        .voltage_mv_cb = app_ble_voltage_mv_cb,
        .disconnected_cb = app_ble_disconnected_cb,
    };

    int ret = ble_peripheral_init(&ble_callbacks);

    if (ret != 0)
    {
        LOG_ERR("ble_peripheral_init failed: %d", ret);
    }

    while (1)
    {
        (void)ble_peripheral_notify_voltage_mv();
        k_sleep(K_MSEC(APP_BLE_NOTIFY_PERIOD_MS));
    }
}

K_THREAD_DEFINE(app_measurements_tid,
                APP_MEASUREMENTS_THREAD_STACK_SIZE,
                app_measurements_thread,
                NULL,
                NULL,
                NULL,
                APP_MEASUREMENTS_THREAD_PRIORITY,
                0,
                0);

K_THREAD_DEFINE(app_move_tid,
                APP_MOVE_THREAD_STACK_SIZE,
                app_move_thread,
                NULL,
                NULL,
                NULL,
                APP_MOVE_THREAD_PRIORITY,
                0,
                0);

K_THREAD_DEFINE(app_ble_tid,
                APP_BLE_THREAD_STACK_SIZE,
                app_ble_thread,
                NULL,
                NULL,
                NULL,
                APP_BLE_THREAD_PRIORITY,
                0,
                0);

int main(void)
{
    voltage_regulator_configure_regout0_3v3();

    while (1)
    {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}
