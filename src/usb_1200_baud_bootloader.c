/*
 * Arduino-style "1200 baud touch": when the host switches the CDC ACM port
 * to 1200 baud, reboot into the Adafruit UF2 bootloader (same as &bootloader).
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/device.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/drivers/uart/cdc_acm.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// Adafruit_nRF52_Bootloader DFU_MAGIC_UF2_RESET, stored in GPREGRET by sys_reboot
#define UF2_RESET_MAGIC 0x57

static void reboot_work_cb(struct k_work *work) { sys_reboot(UF2_RESET_MAGIC); }
static K_WORK_DELAYABLE_DEFINE(reboot_work, reboot_work_cb);

static void rate_cb(const struct device *dev, uint32_t rate)
{
    if (rate == 1200)
    {
        LOG_INF("1200 baud touch, rebooting into bootloader");
        // Let the SET_LINE_CODING control transfer complete first
        k_work_reschedule(&reboot_work, K_MSEC(100));
    }
}

static int usb_1200_baud_bootloader_init(void)
{
    const struct device *dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);

    if (!device_is_ready(dev))
    {
        return -ENODEV;
    }
    return cdc_acm_dte_rate_callback_set(dev, rate_cb);
}

SYS_INIT(usb_1200_baud_bootloader_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
