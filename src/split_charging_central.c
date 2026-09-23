/*
 * Central side: once a half's link is encrypted, find its charging
 * characteristic, read it, subscribe, and raise zmk_split_charging_state_changed
 * with the same source index ZMK uses for that half's battery.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/split_charging.h>
#include <zmk/events/split_charging_state_changed.h>

// Not in a public header, but exported by ZMK's split central (app/src/split/bluetooth/central.c)
extern int peripheral_slot_index_for_conn(struct bt_conn *conn);

#define SLOT_COUNT CONFIG_ZMK_SPLIT_BLE_CENTRAL_PERIPHERALS

static const struct bt_uuid_128 char_uuid = BT_UUID_INIT_128(ZMK_SPLIT_CHARGING_CHAR_UUID);

struct charging_slot
{
    struct bt_conn *conn;
    struct bt_gatt_discover_params discover;
    struct bt_gatt_subscribe_params subscribe;
    struct bt_gatt_read_params read;
    struct k_work_delayable start;
    bool charging;
    bool dirty;
};

static struct charging_slot slots[SLOT_COUNT];

// Raise events from the system workqueue, not the BT RX thread
static void raise_work_cb(struct k_work *work)
{
    for (int i = 0; i < SLOT_COUNT; i++)
    {
        if (slots[i].dirty)
        {
            slots[i].dirty = false;
            raise_zmk_split_charging_state_changed(
                (struct zmk_split_charging_state_changed){.source = i, .charging = slots[i].charging});
        }
    }
}
static K_WORK_DEFINE(raise_work, raise_work_cb);

static void set_charging(struct charging_slot *slot, bool charging)
{
    LOG_INF("Peripheral %d charging: %d", (int)(slot - slots), charging);
    slot->charging = charging;
    slot->dirty = true;
    k_work_submit(&raise_work);
}

static uint8_t notify_cb(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
                         const void *data, uint16_t length)
{
    struct charging_slot *slot = CONTAINER_OF(params, struct charging_slot, subscribe);

    if (!data)
    {
        params->value_handle = 0U;
        return BT_GATT_ITER_STOP;
    }
    if (length > 0)
    {
        set_charging(slot, ((const uint8_t *)data)[0] != 0);
    }
    return BT_GATT_ITER_CONTINUE;
}

static uint8_t read_cb(struct bt_conn *conn, uint8_t err, struct bt_gatt_read_params *params,
                       const void *data, uint16_t length)
{
    struct charging_slot *slot = CONTAINER_OF(params, struct charging_slot, read);

    if (!err && data && length > 0)
    {
        set_charging(slot, ((const uint8_t *)data)[0] != 0);
    }
    return BT_GATT_ITER_STOP;
}

static uint8_t discover_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                           struct bt_gatt_discover_params *params)
{
    struct charging_slot *slot = CONTAINER_OF(params, struct charging_slot, discover);

    if (!attr)
    {
        LOG_WRN("Peripheral %d has no charging characteristic (old firmware?)", (int)(slot - slots));
        return BT_GATT_ITER_STOP;
    }

    const struct bt_gatt_chrc *chrc = attr->user_data;
    // Service layout is fixed: value handle, then its CCC
    slot->subscribe.value_handle = chrc->value_handle;
    slot->subscribe.ccc_handle = chrc->value_handle + 1;
    slot->subscribe.value = BT_GATT_CCC_NOTIFY;
    slot->subscribe.notify = notify_cb;
    int ret = bt_gatt_subscribe(conn, &slot->subscribe);
    if (ret < 0 && ret != -EALREADY)
    {
        LOG_ERR("Charging subscribe failed (%d)", ret);
    }

    slot->read.func = read_cb;
    slot->read.handle_count = 1;
    slot->read.single.handle = chrc->value_handle;
    slot->read.single.offset = 0;
    ret = bt_gatt_read(conn, &slot->read);
    if (ret < 0)
    {
        LOG_ERR("Charging read failed (%d)", ret);
    }

    return BT_GATT_ITER_STOP;
}

static void start_work_cb(struct k_work *work)
{
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct charging_slot *slot = CONTAINER_OF(dwork, struct charging_slot, start);

    if (!slot->conn)
    {
        return;
    }

    slot->discover.uuid = &char_uuid.uuid;
    slot->discover.func = discover_cb;
    slot->discover.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    slot->discover.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    slot->discover.type = BT_GATT_DISCOVER_CHARACTERISTIC;

    int ret = bt_gatt_discover(slot->conn, &slot->discover);
    if (ret < 0)
    {
        LOG_ERR("Charging discovery failed (%d)", ret);
    }
}

static void security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
    struct bt_conn_info info;

    if (err || level < BT_SECURITY_L2 || bt_conn_get_info(conn, &info) < 0 ||
        info.role != BT_CONN_ROLE_CENTRAL)
    {
        return;
    }

    int idx = peripheral_slot_index_for_conn(conn);
    if (idx < 0 || idx >= SLOT_COUNT)
    {
        return;
    }

    struct charging_slot *slot = &slots[idx];
    if (slot->conn)
    {
        bt_conn_unref(slot->conn);
    }
    slot->conn = bt_conn_ref(conn);
    // Let ZMK's own split discovery finish first
    k_work_reschedule(&slot->start, K_MSEC(1500));
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    for (int i = 0; i < SLOT_COUNT; i++)
    {
        if (slots[i].conn == conn)
        {
            k_work_cancel_delayable(&slots[i].start);
            bt_conn_unref(slots[i].conn);
            slots[i].conn = NULL;
            set_charging(&slots[i], false);
        }
    }
}

BT_CONN_CB_DEFINE(split_charging_conn_cb) = {
    .disconnected = disconnected,
    .security_changed = security_changed,
};

static int split_charging_central_init(void)
{
    for (int i = 0; i < SLOT_COUNT; i++)
    {
        k_work_init_delayable(&slots[i].start, start_work_cb);
    }
    return 0;
}

SYS_INIT(split_charging_central_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
