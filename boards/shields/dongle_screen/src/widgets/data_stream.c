/*
 * Data stream: rows of hex that scroll up one row per key press.
 *  - middle row highlighted in the layer colour between > < markers
 *  - "ICE: BREACH" while typing, dims to "ICE: STANDBY" after a while idle
 *  - "PWR: LOW" when a half's battery is low and not charging
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>
#include <zmk/split/central.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/battery_state_changed.h>
#if IS_ENABLED(CONFIG_ZMK_SPLIT_CHARGING)
#include <zmk/events/split_charging_state_changed.h>
#endif

#include "data_stream.h"
#include "../layer_colors.h"
#include "../theme.h"

#define PERIPHERALS ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT
#define BYTES_PER_ROW 6
#define ROW_PITCH 11
#define ROW_X 12
#define WIDTH 136
#define HIGHLIGHT (DATA_STREAM_ROWS / 2)
#define IDLE_ROW_COLOR 0x322C50

// Updated from event context, read when rendering on the display queue
static struct
{
    uint8_t bytes[DATA_STREAM_ROWS][BYTES_PER_ROW];
    uint8_t head; // index of the top row
    bool idle;
    uint8_t layer;
    int8_t level[PERIPHERALS];
    bool charging[PERIPHERALS];
} stream;

static uint32_t rng_state = 0x2545F491;
static struct zmk_widget_data_stream *data_stream;

static uint32_t xorshift32(void)
{
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return rng_state = x;
}

static void fill_row(uint8_t *row)
{
    for (int i = 0; i < BYTES_PER_ROW; i++)
    {
        row[i] = xorshift32() & 0xFF;
    }
}

// Drop the top row and add a fresh one at the bottom
static void push_row(void)
{
    fill_row(stream.bytes[stream.head]);
    stream.head = (stream.head + 1) % DATA_STREAM_ROWS;
}

static bool battery_low(void)
{
    for (int i = 0; i < PERIPHERALS; i++)
    {
        if (stream.level[i] >= 1 && stream.level[i] < CONFIG_DONGLE_SCREEN_LOW_BATTERY_PCT &&
            !stream.charging[i])
        {
            return true;
        }
    }
    return false;
}

static void render_work_cb(struct k_work *work)
{
    if (!data_stream)
    {
        return;
    }

    bool idle = stream.idle;
    uint32_t tint = layer_color(stream.layer);

    for (int r = 0; r < DATA_STREAM_ROWS; r++)
    {
        const uint8_t *b = stream.bytes[(stream.head + r) % DATA_STREAM_ROWS];
        lv_label_set_text_fmt(data_stream->rows[r], "%02X %02X %02X %02X %02X %02X", b[0], b[1], b[2],
                              b[3], b[4], b[5]);

        uint32_t color = r == HIGHLIGHT ? (idle ? IDLE_ROW_COLOR : tint) : CP_DIM;
        lv_obj_set_style_text_color(data_stream->rows[r], CP_COLOR(color), 0);
    }

    if (idle)
    {
        lv_obj_add_flag(data_stream->marker_left, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(data_stream->marker_right, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_clear_flag(data_stream->marker_left, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(data_stream->marker_right, LV_OBJ_FLAG_HIDDEN);
    }

    if (battery_low())
    {
        lv_label_set_text(data_stream->status, "PWR: LOW");
        lv_obj_set_style_text_color(data_stream->status, CP_COLOR(CP_RED), 0);
    }
    else
    {
        lv_label_set_text(data_stream->status, idle ? "ICE: STANDBY" : "ICE: BREACH");
        lv_obj_set_style_text_color(data_stream->status, CP_COLOR(idle ? CP_DIM : CP_YELLOW), 0);
    }
}

static K_WORK_DEFINE(render_work, render_work_cb);

static void request_render(void)
{
    if (data_stream && zmk_display_is_initialized())
    {
        k_work_submit_to_queue(zmk_display_work_q(), &render_work);
    }
}

static void idle_work_cb(struct k_work *work)
{
    stream.idle = true;
    request_render();
}

static K_WORK_DELAYABLE_DEFINE(idle_work, idle_work_cb);

static int data_stream_listener(const zmk_event_t *eh)
{
    const struct zmk_position_state_changed *pos;
    const struct zmk_peripheral_battery_state_changed *bat;

    if ((pos = as_zmk_position_state_changed(eh)))
    {
        if (!pos->state)
        {
            return ZMK_EV_EVENT_BUBBLE;
        }
        rng_state ^= k_cycle_get_32() ^ (pos->position << 16);
        if (rng_state == 0)
        {
            rng_state = 0x2545F491;
        }
        push_row();
        stream.idle = false;
        k_work_reschedule(&idle_work, K_SECONDS(CONFIG_DONGLE_SCREEN_DATA_STREAM_IDLE_S));
    }
    else if (as_zmk_layer_state_changed(eh))
    {
        stream.layer = zmk_keymap_highest_layer_active();
    }
    else if ((bat = as_zmk_peripheral_battery_state_changed(eh)))
    {
        if (bat->source < PERIPHERALS)
        {
            stream.level[bat->source] = bat->state_of_charge;
        }
    }
#if IS_ENABLED(CONFIG_ZMK_SPLIT_CHARGING)
    else
    {
        const struct zmk_split_charging_state_changed *chg = as_zmk_split_charging_state_changed(eh);
        if (chg && chg->source < PERIPHERALS)
        {
            stream.charging[chg->source] = chg->charging;
        }
    }
#endif

    request_render();
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(data_stream, data_stream_listener);
ZMK_SUBSCRIPTION(data_stream, zmk_position_state_changed);
ZMK_SUBSCRIPTION(data_stream, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(data_stream, zmk_peripheral_battery_state_changed);
#if IS_ENABLED(CONFIG_ZMK_SPLIT_CHARGING)
ZMK_SUBSCRIPTION(data_stream, zmk_split_charging_state_changed);
#endif

int zmk_widget_data_stream_init(struct zmk_widget_data_stream *widget, lv_obj_t *parent)
{
    widget->obj = cp_container(parent, 0, 0, WIDTH, DATA_STREAM_ROWS * ROW_PITCH + 16);

    for (int r = 0; r < DATA_STREAM_ROWS; r++)
    {
        fill_row(stream.bytes[r]);
        widget->rows[r] = cp_label(widget->obj, &cp_mono_12, CP_DIM, "");
        lv_obj_set_pos(widget->rows[r], ROW_X, r * ROW_PITCH);
    }

    widget->marker_left = cp_label(widget->obj, &cp_mono_12, CP_YELLOW, ">");
    lv_obj_set_pos(widget->marker_left, 0, HIGHLIGHT * ROW_PITCH);
    widget->marker_right = cp_label(widget->obj, &cp_mono_12, CP_YELLOW, "<");
    lv_obj_align(widget->marker_right, LV_ALIGN_TOP_RIGHT, 0, HIGHLIGHT * ROW_PITCH);

    widget->status = cp_label(widget->obj, &cp_mono_10, CP_YELLOW, "");
    lv_obj_align(widget->status, LV_ALIGN_TOP_MID, 0, DATA_STREAM_ROWS * ROW_PITCH + 3);

    for (int i = 0; i < PERIPHERALS; i++)
    {
        stream.level[i] = -1;
    }
    stream.layer = zmk_keymap_highest_layer_active();

    data_stream = widget;
    // Draw the initial state directly: we're already on the display thread
    render_work_cb(NULL);
    k_work_schedule(&idle_work, K_SECONDS(CONFIG_DONGLE_SCREEN_DATA_STREAM_IDLE_S));
    return 0;
}

lv_obj_t *zmk_widget_data_stream_obj(struct zmk_widget_data_stream *widget)
{
    return widget->obj;
}
