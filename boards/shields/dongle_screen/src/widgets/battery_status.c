/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/services/bas.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/split/central.h>
#include <zmk/display.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/usb.h>

#include "battery_status.h"
#if IS_ENABLED(CONFIG_ZMK_SPLIT_CHARGING)
#include <zmk/events/split_charging_state_changed.h>
#endif
#include "../brightness.h"

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY)
    #define SOURCE_OFFSET 1
#else
    #define SOURCE_OFFSET 0
#endif

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct battery_state {
    uint8_t source;
    uint8_t level;
    bool usb_present;
};

struct battery_object {
    lv_obj_t *symbol;
    lv_obj_t *label;
} battery_objects[ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET];
    
static lv_color_t battery_image_buffer[ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET][102 * 5];

// Peripheral reconnection tracking
// ZMK sends battery events with level < 1 when peripherals disconnect
static int8_t last_battery_levels[ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET];

// Halves on USB power (from zmk_split_charging_state_changed); written in event context
static bool charging[ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET];

#define CHARGING_COLOR lv_palette_main(LV_PALETTE_GREEN)

static void init_peripheral_tracking(void) {
    for (int i = 0; i < (ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET); i++) {
        last_battery_levels[i] = -1; // -1 indicates never seen before
    }
}

static bool is_peripheral_reconnecting(uint8_t source, uint8_t new_level) {
    if (source >= (ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET)) {
        return false;
    }
    
    int8_t previous_level = last_battery_levels[source];
    
    // Reconnection detected if:
    // 1. Previous level was < 1 (disconnected/unknown) AND
    // 2. New level is >= 1 (valid battery level)
    bool reconnecting = (previous_level < 1) && (new_level >= 1);
    
    if (reconnecting) {
        LOG_INF("Peripheral %d reconnection: %d%% -> %d%% (was %s)", 
                source, previous_level, new_level, 
                previous_level == -1 ? "never seen" : "disconnected");
    }
    
    return reconnecting;
}

static void draw_battery(lv_obj_t *canvas, uint8_t level, bool usb_present) {
    
    if (usb_present && level >= 1)
    {
        lv_canvas_fill_bg(canvas, CHARGING_COLOR, LV_OPA_COVER);
    } else if (level < CONFIG_DONGLE_SCREEN_LOW_BATTERY_PCT)
    {
        lv_canvas_fill_bg(canvas, lv_palette_main(LV_PALETTE_RED), LV_OPA_COVER);
    } else {
        lv_canvas_fill_bg(canvas, lv_color_white(), LV_OPA_COVER);
    }

    
    lv_draw_rect_dsc_t rect_fill_dsc;
    lv_draw_rect_dsc_init(&rect_fill_dsc);
    rect_fill_dsc.bg_color = lv_color_black();



    lv_canvas_set_px(canvas, 0, 0, lv_color_black());
    lv_canvas_set_px(canvas, 0, 4, lv_color_black());
    lv_canvas_set_px(canvas, 101, 0, lv_color_black());
    lv_canvas_set_px(canvas, 101, 4, lv_color_black());

    if (level <= 99 && level > 0)
    {
        lv_canvas_draw_rect(canvas, level, 1, 100 - level, 3, &rect_fill_dsc);
        lv_canvas_set_px(canvas, 100, 1, lv_color_black());
        lv_canvas_set_px(canvas, 100, 2, lv_color_black());
        lv_canvas_set_px(canvas, 100, 3, lv_color_black());
    }
    
}

static const char *battery_icon(uint8_t level) {
    if (level >= 90) return LV_SYMBOL_BATTERY_FULL;
    if (level >= 65) return LV_SYMBOL_BATTERY_3;
    if (level >= 40) return LV_SYMBOL_BATTERY_2;
    if (level >= 15) return LV_SYMBOL_BATTERY_1;
    return LV_SYMBOL_BATTERY_EMPTY;
}

// Screen slot for a source: halves are numbered in pairing order, which may not match left/right
static int battery_pos(int source) {
#if IS_ENABLED(CONFIG_DONGLE_SCREEN_BATTERY_REVERSE)
    if (source >= SOURCE_OFFSET) {
        return SOURCE_OFFSET + (ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT - 1 - (source - SOURCE_OFFSET));
    }
#endif
    return source;
}

static char side_letter(int source) {
    if (source < SOURCE_OFFSET) {
        return 'D';
    }
    return battery_pos(source) - SOURCE_OFFSET == 0 ? 'L' : 'R';
}

static void opa_anim_cb(void *obj, int32_t value) { lv_obj_set_style_opa(obj, value, 0); }

// Low battery: pulse the bar until it's charged or charging
static void set_pulse(lv_obj_t *obj, bool on) {
    bool running = lv_anim_get(obj, opa_anim_cb) != NULL;
    if (on == running) {
        return;
    }
    if (on) {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, obj);
        lv_anim_set_exec_cb(&a, opa_anim_cb);
        lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_20);
        lv_anim_set_time(&a, 700);
        lv_anim_set_playback_time(&a, 700);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);
    } else {
        lv_anim_del(obj, opa_anim_cb);
        lv_obj_set_style_opa(obj, LV_OPA_COVER, 0);
    }
}

// Disconnect: blink the label a few times
static void blink(lv_obj_t *obj) {
    lv_anim_del(obj, opa_anim_cb);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, opa_anim_cb);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, 250);
    lv_anim_set_playback_time(&a, 250);
    lv_anim_set_repeat_count(&a, 5);
    lv_anim_start(&a);
}

static void set_battery_symbol(lv_obj_t *widget, struct battery_state state) {
    if (state.source >= ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET) {
        return;
    }
    
    // Check for reconnection using the existing battery level mechanism
    bool reconnecting = is_peripheral_reconnecting(state.source, state.level);
    
    bool disconnected = last_battery_levels[state.source] >= 1 && state.level < 1;

    // Update our tracking
    last_battery_levels[state.source] = state.level;


    // Wake screen on reconnection
    if (reconnecting) {
#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0    
        LOG_INF("Peripheral %d reconnected (battery: %d%%), requesting screen wake", 
                state.source, state.level);
        brightness_wake_screen_on_reconnect();
#else 
        LOG_INF("Peripheral %d reconnected (battery: %d%%)", 
                state.source, state.level);
#endif
    }


    LOG_DBG("source: %d, level: %d, usb: %d", state.source, state.level, state.usb_present);
    lv_obj_t *symbol = battery_objects[state.source].symbol;
    lv_obj_t *label = battery_objects[state.source].label;

    draw_battery(symbol, state.level, state.usb_present);
    
    char side = side_letter(state.source);
    bool low = state.level >= 1 && state.level < CONFIG_DONGLE_SCREEN_LOW_BATTERY_PCT && !state.usb_present;

    if (state.level < 1) {
        lv_obj_set_style_text_color(label, lv_palette_main(LV_PALETTE_RED), 0);
        lv_label_set_text_fmt(label, "%c lost", side);
        if (disconnected) {
            LOG_INF("Peripheral %d disconnected", state.source);
            blink(label);
#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0
            brightness_wake_screen_on_reconnect();
#endif
        }
    } else if (state.usb_present) {
        lv_obj_set_style_text_color(label, CHARGING_COLOR, 0);
        lv_label_set_text_fmt(label, "%c %s " LV_SYMBOL_CHARGE " %u", side, battery_icon(state.level), state.level);
    } else if (low) {
        lv_obj_set_style_text_color(label, lv_palette_main(LV_PALETTE_RED), 0);
        lv_label_set_text_fmt(label, "%c %s %u", side, battery_icon(state.level), state.level);
    } else {
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_label_set_text_fmt(label, "%c %s %u", side, battery_icon(state.level), state.level);
    }
    set_pulse(symbol, low);

    lv_obj_clear_flag(symbol, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(symbol);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(label);

}

void battery_status_update_cb(struct battery_state state) {
    struct zmk_widget_dongle_battery_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_symbol(widget->obj, state); }
}

static struct battery_state peripheral_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev = as_zmk_peripheral_battery_state_changed(eh);
    uint8_t source = ev->source + SOURCE_OFFSET;
    return (struct battery_state){
        .source = source,
        .level = ev->state_of_charge,
        .usb_present = source < ARRAY_SIZE(charging) && charging[source],
    };
}

static struct battery_state central_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    return (struct battery_state) {
        .source = 0,
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    };
}

#if IS_ENABLED(CONFIG_ZMK_SPLIT_CHARGING)
static struct battery_state charging_status_get_state(const zmk_event_t *eh) {
    const struct zmk_split_charging_state_changed *ev = as_zmk_split_charging_state_changed(eh);
    uint8_t source = ev->source + SOURCE_OFFSET;

    if (source >= ARRAY_SIZE(charging)) {
        return (struct battery_state){.source = UINT8_MAX};
    }
    charging[source] = ev->charging;

    // Redraw with the last known level; UINT8_MAX source = nothing to draw yet
    int8_t level = last_battery_levels[source];
    return (struct battery_state){
        .source = level < 0 ? UINT8_MAX : source,
        .level = level < 0 ? 0 : level,
        .usb_present = ev->charging,
    };
}
#endif

static struct battery_state battery_status_get_state(const zmk_event_t *eh) { 
#if IS_ENABLED(CONFIG_ZMK_SPLIT_CHARGING)
    if (eh != NULL && as_zmk_split_charging_state_changed(eh) != NULL) {
        return charging_status_get_state(eh);
    }
#endif
    if (eh != NULL && as_zmk_peripheral_battery_state_changed(eh) != NULL) {
        return peripheral_battery_status_get_state(eh);
    } else {
        return central_battery_status_get_state(eh);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_dongle_battery_status, struct battery_state,
                            battery_status_update_cb, battery_status_get_state)

ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_peripheral_battery_state_changed);
#if IS_ENABLED(CONFIG_ZMK_SPLIT_CHARGING)
ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_split_charging_state_changed);
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY)
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_usb_conn_state_changed);
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
#endif /* !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) */
#endif /* IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY) */

int zmk_widget_dongle_battery_status_init(struct zmk_widget_dongle_battery_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);

    lv_obj_set_size(widget->obj, 240, 40);
    
    for (int i = 0; i < ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET; i++) {
        lv_obj_t *image_canvas = lv_canvas_create(widget->obj);
        lv_obj_t *battery_label = lv_label_create(widget->obj);

        lv_canvas_set_buffer(image_canvas, battery_image_buffer[i], 102, 5, LV_IMG_CF_TRUE_COLOR);

        int pos = battery_pos(i);
        lv_obj_align(image_canvas, LV_ALIGN_BOTTOM_MID, -60 +(pos * 120), -8);
        lv_obj_align(battery_label, LV_ALIGN_TOP_MID, -60 +(pos * 120), 0);

        lv_obj_add_flag(image_canvas, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(battery_label, LV_OBJ_FLAG_HIDDEN);
        
        battery_objects[i] = (struct battery_object){
            .symbol = image_canvas,
            .label = battery_label,
        };
    }

    sys_slist_append(&widgets, &widget->node);

    // Initialize peripheral tracking
    init_peripheral_tracking();

    widget_dongle_battery_status_init();

    return 0;
}

lv_obj_t *zmk_widget_dongle_battery_status_obj(struct zmk_widget_dongle_battery_status *widget) {
    return widget->obj;
}