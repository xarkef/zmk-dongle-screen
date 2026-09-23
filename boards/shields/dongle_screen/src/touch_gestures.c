/*
 * Touch gestures: tap / double-tap / long-press / 4-way swipe on the screen's
 * touch panel, each mapped to a ZMK behavior via a
 * "zmk,dongle-screen-touch-gestures" devicetree node.
 *
 * A touch while the screen is idle-dimmed only wakes it (gesture swallowed),
 * so tapping a dark screen doesn't also pause your music.
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_dongle_screen_touch_gestures

#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/keymap.h>
#include <zmk/events/position_state_changed.h>

#include "brightness.h"

#define GESTURES_NODE DT_DRV_INST(0)

BUILD_ASSERT(DT_PROP_LEN(GESTURES_NODE, bindings) == 7,
             "touch gestures: bindings must list 7 behaviors "
             "(tap, double-tap, long-press, swipe-up, swipe-down, swipe-left, swipe-right)");

enum gesture
{
    GESTURE_TAP,
    GESTURE_DOUBLE_TAP,
    GESTURE_LONG_PRESS,
    GESTURE_SWIPE_UP,
    GESTURE_SWIPE_DOWN,
    GESTURE_SWIPE_LEFT,
    GESTURE_SWIPE_RIGHT,
    GESTURE_COUNT,
};

static const char *const gesture_names[] = {
    "tap", "double-tap", "long-press", "swipe-up", "swipe-down", "swipe-left", "swipe-right",
};

static const struct zmk_behavior_binding bindings[] = {
    LISTIFY(7, ZMK_KEYMAP_EXTRACT_BINDING, (, ), GESTURES_NODE)};

// Virtual key positions well past any real key, one per gesture
#define GESTURE_POSITION_BASE 200

// --- Dispatch: runs behaviors on a dedicated queue so we can hold keys briefly ---

K_THREAD_STACK_DEFINE(gesture_q_stack, 1024);
static struct k_work_q gesture_q;

static struct
{
    struct k_work work;
    enum gesture gesture;
} dispatch;

static void dispatch_work_cb(struct k_work *work)
{
    enum gesture g = dispatch.gesture;
    struct zmk_behavior_binding_event event = {
        .layer = zmk_keymap_highest_layer_active(),
        .position = GESTURE_POSITION_BASE + g,
        .timestamp = k_uptime_get(),
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
        .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
#endif
    };

    LOG_INF("touch gesture: %s", gesture_names[g]);
    zmk_behavior_invoke_binding(&bindings[g], event, true);
    k_msleep(CONFIG_DONGLE_SCREEN_TOUCH_TAP_MS);
    event.timestamp = k_uptime_get();
    zmk_behavior_invoke_binding(&bindings[g], event, false);
}

static void fire(enum gesture g)
{
    dispatch.gesture = g;
    k_work_submit_to_queue(&gesture_q, &dispatch.work);
}

// A single tap is held back until the double-tap window passes
static void pending_tap_cb(struct k_work *work) { fire(GESTURE_TAP); }
static K_WORK_DELAYABLE_DEFINE(pending_tap, pending_tap_cb);

// --- Gesture recognition (input thread) ---

static struct
{
    int32_t raw_x, raw_y;
    int32_t start_x, start_y, last_x, last_y;
    int64_t start_ms;
    bool touching;
    bool swallow;
} t;

static void map_point(int32_t rx, int32_t ry, int32_t *x, int32_t *y)
{
    int32_t a = DT_PROP(GESTURES_NODE, swap_xy) ? ry : rx;
    int32_t b = DT_PROP(GESTURES_NODE, swap_xy) ? rx : ry;
    // Only deltas matter, so inverting is just a sign flip
    *x = DT_PROP(GESTURES_NODE, invert_x) ? -a : a;
    *y = DT_PROP(GESTURES_NODE, invert_y) ? -b : b;
}

static void on_release(void)
{
    int32_t dx = t.last_x - t.start_x;
    int32_t dy = t.last_y - t.start_y;
    int64_t held = k_uptime_get() - t.start_ms;

    LOG_INF("touch end: dx=%d dy=%d held=%lldms", dx, dy, held);

    if (t.swallow)
    {
        return;
    }

    if (abs(dx) >= CONFIG_DONGLE_SCREEN_TOUCH_SWIPE_PX || abs(dy) >= CONFIG_DONGLE_SCREEN_TOUCH_SWIPE_PX)
    {
        k_work_cancel_delayable(&pending_tap);
        if (abs(dx) > abs(dy))
        {
            fire(dx > 0 ? GESTURE_SWIPE_RIGHT : GESTURE_SWIPE_LEFT);
        }
        else
        {
            fire(dy > 0 ? GESTURE_SWIPE_DOWN : GESTURE_SWIPE_UP);
        }
    }
    else if (held >= CONFIG_DONGLE_SCREEN_TOUCH_LONG_PRESS_MS)
    {
        k_work_cancel_delayable(&pending_tap);
        fire(GESTURE_LONG_PRESS);
    }
    else if (k_work_delayable_is_pending(&pending_tap))
    {
        k_work_cancel_delayable(&pending_tap);
        fire(GESTURE_DOUBLE_TAP);
    }
    else
    {
        k_work_schedule(&pending_tap, K_MSEC(CONFIG_DONGLE_SCREEN_TOUCH_DOUBLE_TAP_MS));
    }
}

static void touch_input_cb(struct input_event *evt)
{
    switch (evt->code)
    {
    case INPUT_ABS_X:
        t.raw_x = evt->value;
        break;
    case INPUT_ABS_Y:
        t.raw_y = evt->value;
        break;
    case INPUT_BTN_TOUCH:
        if (evt->value && !t.touching)
        {
            t.touching = true;
            t.start_ms = k_uptime_get();
            map_point(t.raw_x, t.raw_y, &t.start_x, &t.start_y);
            t.last_x = t.start_x;
            t.last_y = t.start_y;
            t.swallow = brightness_wake_screen_on_touch();
            LOG_INF("touch start: raw x=%d y=%d%s", t.raw_x, t.raw_y, t.swallow ? " (wake only)" : "");
        }
        else if (evt->value)
        {
            map_point(t.raw_x, t.raw_y, &t.last_x, &t.last_y);
        }
        else if (t.touching)
        {
            t.touching = false;
            on_release();
        }
        break;
    default:
        break;
    }
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_PHANDLE(GESTURES_NODE, input)), touch_input_cb);

static int touch_gestures_init(void)
{
    k_work_queue_start(&gesture_q, gesture_q_stack, K_THREAD_STACK_SIZEOF(gesture_q_stack),
                       CONFIG_SYSTEM_WORKQUEUE_PRIORITY, NULL);
    k_work_init(&dispatch.work, dispatch_work_cb);
    return 0;
}

SYS_INIT(touch_gestures_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
