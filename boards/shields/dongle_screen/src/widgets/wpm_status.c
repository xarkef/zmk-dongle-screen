/*
 * WPM: label, zero-padded number and a meter of the last WPM_BARS readings.
 *
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/wpm_state_changed.h>

#include "wpm_status.h"
#include "../theme.h"

#define BAR_W 3
#define BAR_GAP 2
#define BAR_MAX_H 12
#define BAR_BOTTOM 50

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

// Newest reading last; only touched on the display queue
static uint8_t history[WPM_BARS];

struct wpm_status_state
{
    int wpm;
};

static struct wpm_status_state get_state(const zmk_event_t *_eh)
{
    const struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(_eh);

    return (struct wpm_status_state){
        .wpm = ev ? ev->state : 0};
}

static void set_wpm(struct zmk_widget_wpm_status *widget, struct wpm_status_state state)
{
    lv_label_set_text_fmt(widget->wpm_label, "%03d", MIN(state.wpm, 999));

    for (int i = 0; i < WPM_BARS; i++)
    {
        int h = CLAMP(1 + history[i] / 10, 1, BAR_MAX_H);
        lv_obj_set_height(widget->bars[i], h);
        lv_obj_set_y(widget->bars[i], BAR_BOTTOM - h);
        lv_obj_set_style_bg_color(widget->bars[i], CP_COLOR(history[i] ? CP_YELLOW : CP_DIM), 0);
    }
}

static void wpm_status_update_cb(struct wpm_status_state state)
{
    memmove(history, history + 1, WPM_BARS - 1);
    history[WPM_BARS - 1] = MIN(state.wpm, 255);

    struct zmk_widget_wpm_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node)
    {
        set_wpm(widget, state);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_wpm_status, struct wpm_status_state,
                            wpm_status_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_wpm_status, zmk_wpm_state_changed);

int zmk_widget_wpm_status_init(struct zmk_widget_wpm_status *widget, lv_obj_t *parent)
{
    widget->obj = cp_container(parent, 0, 0, 64, 52);

    cp_label(widget->obj, &cp_mono_12, CP_YELLOW, "WPM");
    widget->wpm_label = cp_label(widget->obj, &cp_orbitron_22, CP_TEXT, "000");
    lv_obj_set_pos(widget->wpm_label, 0, 12);

    for (int i = 0; i < WPM_BARS; i++)
    {
        lv_obj_t *bar = lv_obj_create(widget->obj);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_set_style_radius(bar, 0, 0);
        lv_obj_set_style_pad_all(bar, 0, 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_size(bar, BAR_W, 1);
        lv_obj_set_x(bar, i * (BAR_W + BAR_GAP));
        widget->bars[i] = bar;
    }

    sys_slist_append(&widgets, &widget->node);
    widget_wpm_status_init();
    return 0;
}

lv_obj_t *zmk_widget_wpm_status_obj(struct zmk_widget_wpm_status *widget)
{
    return widget->obj;
}
