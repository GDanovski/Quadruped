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

#ifndef BLE_PERIPHERAL_BLE_PERIPHERAL_H_
#define BLE_PERIPHERAL_BLE_PERIPHERAL_H_

#include <errno.h>
#include <stdint.h>
#include <zephyr/bluetooth/uuid.h>

#include "move_controller/move_controller.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief BLE service UUID. */
#define BT_UUID_QUADRUPED_BLE_VAL BT_UUID_128_ENCODE(0xfab12400, 0xfae2, 0x4090, 0x8a20, 0x000000000001ULL)

/** @brief Voltage characteristic UUID (read, millivolts). */
#define BT_UUID_QUADRUPED_BLE_VOLTAGE_VAL BT_UUID_128_ENCODE(0xfab12400, 0xfae2, 0x4090, 0x8a20, 0x000000000002ULL)

/** @brief Movement characteristic UUID (write enum value). */
#define BT_UUID_QUADRUPED_BLE_MOVEMENT_VAL BT_UUID_128_ENCODE(0xfab12400, 0xfae2, 0x4090, 0x8a20, 0x000000000003ULL)

#define BT_UUID_QUADRUPED_BLE BT_UUID_DECLARE_128(BT_UUID_QUADRUPED_BLE_VAL)
#define BT_UUID_QUADRUPED_BLE_VOLTAGE BT_UUID_DECLARE_128(BT_UUID_QUADRUPED_BLE_VOLTAGE_VAL)
#define BT_UUID_QUADRUPED_BLE_MOVEMENT BT_UUID_DECLARE_128(BT_UUID_QUADRUPED_BLE_MOVEMENT_VAL)

    /** @brief Callback type for movement writes from central. */
    typedef void (*ble_movement_cb_t)(enum move_command movement);

    /** @brief Callback type for voltage reads from central. */
    typedef int32_t (*ble_voltage_mv_cb_t)(void);

    /** @brief Callback type for BLE disconnect events. */
    typedef void (*ble_disconnected_cb_t)(void);

    /** @brief Callback struct used by the BLE peripheral service. */
    struct ble_peripheral_cb
    {
        ble_movement_cb_t movement_cb;
        ble_voltage_mv_cb_t voltage_mv_cb;
        ble_disconnected_cb_t disconnected_cb;
    };

/**
 * @brief Initialize the BLE peripheral, enable the BT stack and start advertising.
 *
 * Must be called once from application context (thread) before any other API.
 *
 * @param callbacks Struct containing optional callback functions used by the
 *                  service. Can be NULL.
 * @return 0 on success, negative errno otherwise.
 */
#if defined(CONFIG_BLE_PERIPHERAL_MODULE)
    int ble_peripheral_init(struct ble_peripheral_cb *callbacks);
    int ble_peripheral_notify_voltage_mv(void);
    bool ble_peripheral_is_connected(void);

#else /* CONFIG_BLE_PERIPHERAL_MODULE not set */

static inline int ble_peripheral_init(struct ble_peripheral_cb *callbacks)
{
    ARG_UNUSED(callbacks);
    return -ENOTSUP;
}

static inline int ble_peripheral_notify_voltage_mv(void)
{
    return -ENOTSUP;
}

static inline bool ble_peripheral_is_connected(void)
{
    return false;
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* BLE_PERIPHERAL_BLE_PERIPHERAL_H_ */
