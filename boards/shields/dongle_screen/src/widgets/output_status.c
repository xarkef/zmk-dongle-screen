/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/usb.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>

#include "output_status.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

#define CHIP_W 44
#define CHIP_H 18

static lv_color_t chip_buf[CHIP_W * CHIP_H];

struct output_status_state
{
    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
    bool usb_is_hid_ready;
};

static struct output_status_state get_state(const zmk_event_t *_eh)
{
    return (struct output_status_state){
        .selected_endpoint = zmk_endpoints_selected(),                     // 0 = USB , 1 = BLE
        .active_profile_index = zmk_ble_active_profile_index(),            // 0-3 BLE profiles
        .active_profile_connected = zmk_ble_active_profile_is_connected(), // 0 = not connected, 1 = connected
        .active_profile_bonded = !zmk_ble_active_profile_is_open(),        // 0 =  BLE not bonded, 1 = bonded
        .usb_is_hid_ready = zmk_usb_is_hid_ready()};                       // 0 = not ready, 1 = ready
}

static void set_status_symbol(struct zmk_widget_output_status *widget, struct output_status_state state)
{
    bool usb = state.selected_endpoint.transport == ZMK_TRANSPORT_USB;
    // Selected output in the chip: cyan when it's up, red when it isn't
    bool up = usb ? state.usb_is_hid_ready : state.active_profile_connected;

    lv_label_set_text(widget->transport.label, usb ? "USB" : "BLE");
    cp_chip_set(&widget->transport, up ? CP_CYAN : CP_RED, false);

    // BLE profile: green connected, cyan bonded, dim open
    uint32_t ble_color = state.active_profile_connected ? CP_GREEN
                         : state.active_profile_bonded  ? CP_CYAN
                                                        : CP_DIM;
    lv_label_set_text_fmt(widget->ble_label, "BLE %d", state.active_profile_index + 1);
    lv_obj_set_style_text_color(widget->ble_label, CP_COLOR(ble_color), 0);
}

static void output_status_update_cb(struct output_status_state state)
{
    struct zmk_widget_output_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node)
    {
        set_status_symbol(widget, state);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
                            output_status_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);

// output_status.c
int zmk_widget_output_status_init(struct zmk_widget_output_status *widget, lv_obj_t *parent)
{
    widget->obj = cp_container(parent, 0, 0, CHIP_W, 36);

    cp_chip_init(&widget->transport, widget->obj, chip_buf, 0, 0, CHIP_W, CHIP_H, 5, &cp_mono_14, "USB");
    widget->ble_label = cp_label(widget->obj, &cp_mono_10, CP_DIM, "BLE 1");
    lv_obj_align(widget->ble_label, LV_ALIGN_TOP_RIGHT, 0, 22);

    sys_slist_append(&widgets, &widget->node);
    widget_output_status_init();
    return 0;
}

lv_obj_t *zmk_widget_output_status_obj(struct zmk_widget_output_status *widget)
{
    return widget->obj;
}
