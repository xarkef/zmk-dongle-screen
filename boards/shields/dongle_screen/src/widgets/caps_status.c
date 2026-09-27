/*
 * Caps Lock badge, driven by the host's keyboard LED report.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/hid_indicators.h>
#include <zmk/events/hid_indicators_changed.h>

#include "caps_status.h"

#define CHIP_W 42
#define CHIP_H 15

static lv_color_t chip_buf[CHIP_W * CHIP_H];

// HID LED usage 0x02 (Caps Lock) -> bit 1 of the indicator report
#define CAPS_LOCK_BIT BIT(1)

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct caps_status_state
{
    bool caps;
};

static struct caps_status_state get_state(const zmk_event_t *eh)
{
    return (struct caps_status_state){
        .caps = (zmk_hid_indicators_get_current_profile() & CAPS_LOCK_BIT) != 0,
    };
}

static void caps_status_update_cb(struct caps_status_state state)
{
    struct zmk_widget_caps_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node)
    {
        if (state.caps)
        {
            lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_caps_status, struct caps_status_state,
                            caps_status_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_caps_status, zmk_hid_indicators_changed);

int zmk_widget_caps_status_init(struct zmk_widget_caps_status *widget, lv_obj_t *parent)
{
    widget->obj = cp_container(parent, 0, 0, CHIP_W, CHIP_H);
    cp_chip_init(&widget->chip, widget->obj, chip_buf, 0, 0, CHIP_W, CHIP_H, 4, &cp_mono_12, "CAPS");
    cp_chip_set(&widget->chip, CP_YELLOW, true);
    lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);

    sys_slist_append(&widgets, &widget->node);
    widget_caps_status_init();
    return 0;
}

lv_obj_t *zmk_widget_caps_status_obj(struct zmk_widget_caps_status *widget)
{
    return widget->obj;
}
