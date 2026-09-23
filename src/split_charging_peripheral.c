/*
 * Peripheral side: poll VBUS (USB power present = charging) and expose it as a
 * read/notify GATT characteristic for the central.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/bluetooth/gatt.h>
#include <hal/nrf_power.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/split_charging.h>

static uint8_t usb_powered;

static ssize_t read_usb_powered(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
                                uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &usb_powered, sizeof(usb_powered));
}

static void ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) {}

BT_GATT_SERVICE_DEFINE(split_charging_svc,
                       BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(ZMK_SPLIT_CHARGING_SERVICE_UUID)),
                       BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(ZMK_SPLIT_CHARGING_CHAR_UUID),
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ_ENCRYPT, read_usb_powered, NULL, NULL),
                       BT_GATT_CCC(ccc_changed, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT));

static void poll_work_cb(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(poll_work, poll_work_cb);

static void poll_work_cb(struct k_work *work)
{
    uint8_t now = nrf_power_usbregstatus_vbusdet_get(NRF_POWER) ? 1 : 0;

    if (now != usb_powered)
    {
        usb_powered = now;
        LOG_INF("USB power %s", now ? "connected" : "removed");
        bt_gatt_notify(NULL, &split_charging_svc.attrs[1], &usb_powered, sizeof(usb_powered));
    }

    k_work_schedule(&poll_work, K_MSEC(CONFIG_ZMK_SPLIT_CHARGING_POLL_MS));
}

static int split_charging_peripheral_init(void)
{
    usb_powered = nrf_power_usbregstatus_vbusdet_get(NRF_POWER) ? 1 : 0;
    k_work_schedule(&poll_work, K_MSEC(CONFIG_ZMK_SPLIT_CHARGING_POLL_MS));
    return 0;
}

SYS_INIT(split_charging_peripheral_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
