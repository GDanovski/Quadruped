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
#include <stdint.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/atomic.h>

#include "ble_peripheral/ble_peripheral.h"

LOG_MODULE_REGISTER(ble_peripheral, CONFIG_BLE_PERIPHERAL_MODULE_LOG_LEVEL);

#define BLE_ADV_RETRY_DELAY_MS 300u
#define BLE_ADV_RETRY_MAX_COUNT 20u

/* -------------------------------------------------------------------------
 * Module-internal state
 * ------------------------------------------------------------------------- */
static struct ble_peripheral_cb ble_cb;
static atomic_t ble_voltage_notify_enabled = ATOMIC_INIT(0);
static atomic_t ble_adv_retry_count = ATOMIC_INIT(0);
static atomic_t ble_adv_retry_work_ready = ATOMIC_INIT(0);
static atomic_t ble_whitelist_has_entries = ATOMIC_INIT(0);

static struct bt_conn *ble_current_conn;
static struct k_work_delayable ble_adv_retry_work;
#if defined(CONFIG_BT_USER_PHY_UPDATE)
static const struct bt_conn_le_phy_param ble_preferred_phy = {
    .options = BT_CONN_LE_PHY_OPT_NONE,
    .pref_tx_phy = BT_GAP_LE_PHY_CODED,
    .pref_rx_phy = BT_GAP_LE_PHY_CODED,
};
#endif

/* -------------------------------------------------------------------------
 * Advertising payloads
 * ------------------------------------------------------------------------- */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA(BT_DATA_NAME_COMPLETE,
            CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_QUADRUPED_BLE_VAL),
};

static void ble_adv_retry_work_handler(struct k_work *work);

static void ble_add_bond_to_accept_list(const struct bt_bond_info *info, void *user_data)
{
    int *count = (int *)user_data;
    int ret = bt_le_filter_accept_list_add(&info->addr);

    if (ret == 0 || ret == -EALREADY)
    {
        (*count)++;
        return;
    }

    char addr_str[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(&info->addr, addr_str, sizeof(addr_str));
    LOG_WRN("Failed to add bonded peer %s to accept list: %d", addr_str, ret);
}

static void ble_reload_accept_list_from_bonds(void)
{
    int ret = bt_le_filter_accept_list_clear();

    if (ret != 0)
    {
        LOG_WRN("Failed to clear accept list: %d", ret);
        return;
    }

    int count = 0;

    bt_foreach_bond(BT_ID_DEFAULT, ble_add_bond_to_accept_list, &count);
    atomic_set(&ble_whitelist_has_entries, (count > 0) ? 1 : 0);
    LOG_INF("Accept list loaded with %d bonded peer(s)", count);
}

static int ble_start_advertising(void)
{
    bool use_accept_list = (atomic_get(&ble_whitelist_has_entries) != 0);
    const struct bt_le_adv_param *adv_param = use_accept_list
                                                  ? BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONN | BT_LE_ADV_OPT_FILTER_CONN,
                                                                    BT_GAP_ADV_FAST_INT_MIN_2,
                                                                    BT_GAP_ADV_FAST_INT_MAX_2,
                                                                    NULL)
                                                  : BT_LE_ADV_CONN_FAST_1;
    int ret = bt_le_adv_start(adv_param,
                              ad, ARRAY_SIZE(ad),
                              sd, ARRAY_SIZE(sd));

    if (ret == -EALREADY)
    {
        LOG_DBG("Advertising already running");
        return 0;
    }

    if (ret != 0)
    {
        LOG_ERR("Failed to start advertising: %d", ret);
        return ret;
    }

    LOG_INF("Advertising started (%s)", use_accept_list ? "accept-list" : "open");
    return 0;
}

static void ble_schedule_advertising_retry(void)
{
    if (atomic_get(&ble_adv_retry_work_ready) == 0)
    {
        return;
    }

    int retries = (int)atomic_get(&ble_adv_retry_count);

    if (retries >= (int)BLE_ADV_RETRY_MAX_COUNT)
    {
        LOG_ERR("Advertising retry limit reached (%u)", BLE_ADV_RETRY_MAX_COUNT);
        return;
    }

    retries = (int)atomic_inc(&ble_adv_retry_count) + 1;
    LOG_WRN("Advertising start deferred (attempt %d/%u)", retries, BLE_ADV_RETRY_MAX_COUNT);
    (void)k_work_reschedule(&ble_adv_retry_work, K_MSEC(BLE_ADV_RETRY_DELAY_MS));
}

static void ble_ensure_advertising(void)
{
    int ret = ble_start_advertising();

    if (ret == 0)
    {
        atomic_set(&ble_adv_retry_count, 0);
        return;
    }

    if (ret == -ENOMEM || ret == -EBUSY)
    {
        ble_schedule_advertising_retry();
    }
}

static void ble_adv_retry_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    ble_ensure_advertising();
}

static void ble_request_coded_phy(struct bt_conn *conn)
{
#if defined(CONFIG_BT_USER_PHY_UPDATE)
    int ret;

    if (conn == NULL)
    {
        return;
    }

    LOG_DBG("Requesting LE Coded PHY preference (TX/RX)");
    ret = bt_conn_le_phy_update(conn, &ble_preferred_phy);
    if (ret == 0)
    {
        LOG_DBG("PHY preference request accepted by local controller");
        return;
    }

    if (ret == -ENOTSUP || ret == -EINVAL || ret == -ENOMEM || ret == -EBUSY)
    {
        LOG_DBG("LE Coded PHY preference not applied, keeping default PHY (err %d)", ret);
        return;
    }

    LOG_DBG("PHY preference request failed, keeping default PHY (err %d)", ret);
#else
    ARG_UNUSED(conn);
    LOG_DBG("LE Coded PHY preference unavailable in this build; keeping default PHY");
#endif
}

static size_t voltage_mv_to_string(char *out, size_t out_size)
{
    int32_t mv = 0;

    if (ble_cb.voltage_mv_cb != NULL)
    {
        mv = ble_cb.voltage_mv_cb();
    }

    int written = snprintk(out, out_size, "%d", (int)mv);

    if (written < 0)
    {
        out[0] = '\0';
        return 0u;
    }

    if ((size_t)written >= out_size)
    {
        return out_size - 1u;
    }

    return (size_t)written;
}

/* -------------------------------------------------------------------------
 * GATT callbacks
 * ------------------------------------------------------------------------- */
static ssize_t voltage_read_cb(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr,
                               void *buf, uint16_t len, uint16_t offset)
{
    char voltage_str[16];
    size_t voltage_len = voltage_mv_to_string(voltage_str, sizeof(voltage_str));

    ARG_UNUSED(attr);

    return bt_gatt_attr_read(conn, attr, buf, len, offset, voltage_str, voltage_len);
}

static ssize_t move_cmd_write_cb(struct bt_conn *conn,
                                 const struct bt_gatt_attr *attr,
                                 const void *buf, uint16_t len,
                                 uint16_t offset, uint8_t flags)
{
    ARG_UNUSED(conn);
    ARG_UNUSED(attr);
    ARG_UNUSED(flags);

    if (len != sizeof(uint8_t))
    {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    if (offset != 0u)
    {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    uint8_t cmd = *((const uint8_t *)buf);

    if (cmd >= MOVE_COMMAND_COUNT)
    {
        return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
    }

    if (ble_cb.movement_cb != NULL)
    {
        ble_cb.movement_cb((enum move_command)cmd);
    }

    LOG_DBG("Received movement command: %u", cmd);

    return (ssize_t)len;
}

static void voltage_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    ARG_UNUSED(attr);

    bool enabled = (value == BT_GATT_CCC_NOTIFY);

    atomic_set(&ble_voltage_notify_enabled, enabled ? 1 : 0);
    LOG_DBG("Voltage notifications %s", enabled ? "enabled" : "disabled");
}

/* -------------------------------------------------------------------------
 * GATT service definition
 * ------------------------------------------------------------------------- */
BT_GATT_SERVICE_DEFINE(ble_peripheral_svc,
                       BT_GATT_PRIMARY_SERVICE(BT_UUID_QUADRUPED_BLE),

                       BT_GATT_CHARACTERISTIC(BT_UUID_QUADRUPED_BLE_VOLTAGE,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ_ENCRYPT,
                                              voltage_read_cb, NULL, NULL),
                       BT_GATT_CCC(voltage_ccc_cfg_changed,
                                   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

                       BT_GATT_CHARACTERISTIC(BT_UUID_QUADRUPED_BLE_MOVEMENT,
                                              BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                                              BT_GATT_PERM_WRITE_ENCRYPT,
                                              NULL, move_cmd_write_cb, NULL), );

/* -------------------------------------------------------------------------
 * Connection callbacks
 * ------------------------------------------------------------------------- */
static void connected_cb(struct bt_conn *conn, uint8_t err)
{
    if (err != 0u)
    {
        LOG_ERR("Connection failed (err 0x%02x)", err);

        /* Keep peripheral reconnectable if connect procedure aborts/fails. */
        ble_ensure_advertising();
        return;
    }

    if (atomic_get(&ble_adv_retry_work_ready) != 0)
    {
        (void)k_work_cancel_delayable(&ble_adv_retry_work);
    }
    atomic_set(&ble_adv_retry_count, 0);

    if (ble_current_conn != NULL)
    {
        bt_conn_unref(ble_current_conn);
    }

    ble_current_conn = bt_conn_ref(conn);
    LOG_INF("Central connected");

    /* Upgrade link to encrypted (unauthenticated / just-works pairing) */
    int ret = bt_conn_set_security(conn, BT_SECURITY_L2);

    if (ret != 0)
    {
        LOG_ERR("Failed to request encryption: %d", ret);
    }

    ble_request_coded_phy(conn);
}

static void disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
    ARG_UNUSED(conn);

    LOG_INF("Central disconnected (reason 0x%02x)", reason);

    if (ble_current_conn != NULL)
    {
        bt_conn_unref(ble_current_conn);
        ble_current_conn = NULL;
    }

    atomic_set(&ble_voltage_notify_enabled, 0);

    if (ble_cb.disconnected_cb != NULL)
    {
        ble_cb.disconnected_cb();
    }

    ble_ensure_advertising();
}

static void security_changed_cb(struct bt_conn *conn,
                                bt_security_t level,
                                enum bt_security_err err)
{
    ARG_UNUSED(conn);

    if (err != BT_SECURITY_ERR_SUCCESS)
    {
        LOG_WRN("Security update failed (err %d)", err);
    }
    else
    {
        LOG_INF("Security level changed to %d", level);
    }
}

#if defined(CONFIG_BT_USER_PHY_UPDATE)
static void le_phy_updated_cb(struct bt_conn *conn,
                              struct bt_conn_le_phy_info *param)
{
    ARG_UNUSED(conn);

    LOG_DBG("PHY update callback: TX PHY 0x%02x, RX PHY 0x%02x", param->tx_phy, param->rx_phy);

    if (param->tx_phy == BT_GAP_LE_PHY_CODED || param->rx_phy == BT_GAP_LE_PHY_CODED)
    {
        LOG_DBG("LE Coded PHY active on at least one direction");
        return;
    }

    LOG_DBG("Peer/controller kept uncoded PHY; continuing without LE Coded");
}
#endif

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected_cb,
    .disconnected = disconnected_cb,
    .security_changed = security_changed_cb,
#if defined(CONFIG_BT_USER_PHY_UPDATE)
    .le_phy_updated = le_phy_updated_cb,
#endif
};

/* -------------------------------------------------------------------------
 * Pairing info callbacks (logging only)
 * ------------------------------------------------------------------------- */
static void pairing_complete_cb(struct bt_conn *conn, bool bonded)
{
    LOG_INF("Pairing complete (bonded: %d)", (int)bonded);

    if (!bonded)
    {
        return;
    }

    const bt_addr_le_t *peer = bt_conn_get_dst(conn);

    if (peer == NULL)
    {
        return;
    }

    int ret = bt_le_filter_accept_list_add(peer);

    if (ret == 0 || ret == -EALREADY)
    {
        atomic_set(&ble_whitelist_has_entries, 1);
    }
    else
    {
        char addr_str[BT_ADDR_LE_STR_LEN];

        bt_addr_le_to_str(peer, addr_str, sizeof(addr_str));
        LOG_WRN("Failed to add bonded peer %s to accept list: %d", addr_str, ret);
    }
}

static void pairing_failed_cb(struct bt_conn *conn,
                              enum bt_security_err reason)
{
    ARG_UNUSED(conn);
    LOG_WRN("Pairing failed (reason %d)", reason);
}

static struct bt_conn_auth_info_cb auth_info_cb = {
    .pairing_complete = pairing_complete_cb,
    .pairing_failed = pairing_failed_cb,
};

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */
int ble_peripheral_init(struct ble_peripheral_cb *callbacks)
{
    if (callbacks != NULL)
    {
        ble_cb.movement_cb = callbacks->movement_cb;
        ble_cb.voltage_mv_cb = callbacks->voltage_mv_cb;
        ble_cb.disconnected_cb = callbacks->disconnected_cb;
    }

    k_work_init_delayable(&ble_adv_retry_work, ble_adv_retry_work_handler);
    atomic_set(&ble_adv_retry_work_ready, 1);

    int ret = bt_conn_auth_info_cb_register(&auth_info_cb);

    if (ret != 0)
    {
        LOG_ERR("bt_conn_auth_info_cb_register failed: %d", ret);
        return ret;
    }

    ret = bt_enable(NULL);
    if (ret != 0)
    {
        LOG_ERR("bt_enable failed: %d", ret);
        return ret;
    }

    ret = settings_load();
    if (ret != 0)
    {
        LOG_ERR("settings_load failed: %d", ret);
        return ret;
    }

    ble_reload_accept_list_from_bonds();

    ble_ensure_advertising();

    LOG_INF("BLE peripheral advertising as \"%s\"", CONFIG_BT_DEVICE_NAME);
    return 0;
}

int ble_peripheral_notify_voltage_mv(void)
{
    if (atomic_get(&ble_voltage_notify_enabled) == 0)
    {
        return 0;
    }

    char voltage_str[16];
    size_t voltage_len = voltage_mv_to_string(voltage_str, sizeof(voltage_str));

    int ret = bt_gatt_notify(NULL,
                             &ble_peripheral_svc.attrs[2],
                             voltage_str,
                             voltage_len);

    if (ret != 0 && ret != -ENOTCONN)
    {
        LOG_ERR("bt_gatt_notify failed: %d", ret);
    }

    return ret;
}
