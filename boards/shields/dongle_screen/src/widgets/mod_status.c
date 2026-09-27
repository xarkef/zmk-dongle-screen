/*
 * Modifiers: CTL / SFT / ALT / GUI chips, filled in the layer colour while held.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>
#include <lvgl.h>
#include "mod_status.h"
#include "../layer_colors.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define CHIP_W 36
#define CHIP_H 16
#define CHIP_GAP 6

static const char *const names[MOD_CHIPS] = {"CTL", "SFT", "ALT", "GUI"};
static const uint8_t masks[MOD_CHIPS] = {
    MOD_LCTL | MOD_RCTL,
    MOD_LSFT | MOD_RSFT,
    MOD_LALT | MOD_RALT,
    MOD_LGUI | MOD_RGUI,
};

static lv_color_t chip_bufs[MOD_CHIPS][CHIP_W * CHIP_H];
static struct zmk_widget_mod_status *mod_widget;

static void update_work_cb(struct k_work *work)
{
    uint8_t mods = zmk_hid_get_keyboard_report()->body.modifiers;
    uint32_t tint = layer_color(zmk_keymap_highest_layer_active());

    for (int i = 0; i < MOD_CHIPS; i++)
    {
        bool on = mods & masks[i];
        cp_chip_set(&mod_widget->chips[i], on ? tint : CP_DIM, on);
    }
}

static K_WORK_DEFINE(update_work, update_work_cb);

// Poll the HID report; LVGL is only touched on the display queue
static void mod_status_timer_cb(struct k_timer *timer)
{
    k_work_submit_to_queue(zmk_display_work_q(), &update_work);
}

static K_TIMER_DEFINE(mod_status_timer, mod_status_timer_cb, NULL);

int zmk_widget_mod_status_init(struct zmk_widget_mod_status *widget, lv_obj_t *parent)
{
    widget->obj = cp_container(parent, 0, 0, MOD_CHIPS * (CHIP_W + CHIP_GAP) - CHIP_GAP, CHIP_H);

    for (int i = 0; i < MOD_CHIPS; i++)
    {
        cp_chip_init(&widget->chips[i], widget->obj, chip_bufs[i], i * (CHIP_W + CHIP_GAP), 0, CHIP_W,
                     CHIP_H, 4, &cp_mono_12, names[i]);
    }

    mod_widget = widget;
    update_work_cb(NULL);
    k_timer_start(&mod_status_timer, K_MSEC(100), K_MSEC(100));
    return 0;
}

lv_obj_t *zmk_widget_mod_status_obj(struct zmk_widget_mod_status *widget)
{
    return widget->obj;
}
