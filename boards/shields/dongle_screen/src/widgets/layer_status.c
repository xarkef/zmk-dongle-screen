/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include "layer_status.h"
#include <zmk/events/layer_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>

#include "../layer_colors.h"
#include "../theme.h"

#define GHOST_OFFSET 2

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct layer_status_state
{
    uint8_t index;
    const char *label;
};

static void set_layer_symbol(struct zmk_widget_layer_status *widget, struct layer_status_state state)
{
    uint32_t tint = layer_color(state.index);
    char text[13] = {};

    if (state.label == NULL)
    {
        snprintf(text, sizeof(text), "%i", state.index);
    }
    else
    {
        snprintf(text, sizeof(text), "%s", state.label);
    }

    lv_label_set_text_fmt(widget->tag, "// LAYER_%02d", state.index);

    // Chromatic-aberration ghosts behind the name, never the same colour as the name
    lv_obj_t *labels[] = {widget->ghost_left, widget->ghost_right, widget->name};
    for (int i = 0; i < ARRAY_SIZE(labels); i++)
    {
        lv_label_set_text(labels[i], text);
    }
    lv_obj_set_style_text_color(widget->ghost_left, CP_COLOR(tint == CP_CYAN ? CP_MAGENTA : CP_CYAN), 0);
    lv_obj_set_style_text_color(widget->ghost_right, CP_COLOR(tint == CP_MAGENTA ? CP_CYAN : CP_MAGENTA), 0);
    lv_obj_set_style_text_color(widget->name, CP_COLOR(tint), 0);
    lv_obj_align(widget->ghost_left, LV_ALIGN_TOP_MID, -GHOST_OFFSET, 18);
    lv_obj_align(widget->ghost_right, LV_ALIGN_TOP_MID, GHOST_OFFSET, 18);
    lv_obj_align(widget->name, LV_ALIGN_TOP_MID, 0, 18);
}

static void layer_status_update_cb(struct layer_status_state state)
{
    struct zmk_widget_layer_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_layer_symbol(widget, state); }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh)
{
    uint8_t index = zmk_keymap_highest_layer_active();
    return (struct layer_status_state){
        .index = index,
        .label = zmk_keymap_layer_name(index)};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state, layer_status_update_cb,
                            layer_status_get_state)

ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

int zmk_widget_layer_status_init(struct zmk_widget_layer_status *widget, lv_obj_t *parent)
{
    widget->obj = cp_container(parent, 0, 0, lv_disp_get_hor_res(NULL), 56);

    widget->tag = cp_label(widget->obj, &cp_mono_10, CP_DIM, "");
    lv_obj_align(widget->tag, LV_ALIGN_TOP_MID, 0, 0);
    widget->ghost_left = cp_label(widget->obj, &cp_orbitron_34, CP_CYAN, "");
    widget->ghost_right = cp_label(widget->obj, &cp_orbitron_34, CP_MAGENTA, "");
    widget->name = cp_label(widget->obj, &cp_orbitron_34, CP_MAGENTA, "");

    sys_slist_append(&widgets, &widget->node);
    widget_layer_status_init();
    return 0;
}

lv_obj_t *zmk_widget_layer_status_obj(struct zmk_widget_layer_status *widget)
{
    return widget->obj;
}