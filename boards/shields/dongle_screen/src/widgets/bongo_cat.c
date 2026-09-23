/*
 * Bongo cat:
 *  - slaps the desk with alternating paws on every key press
 *  - falls asleep (closed eyes + zzz) after a while without typing
 *  - sweats when typing fast (WPM threshold)
 *  - frowns when a half's battery is low and not charging
 *  - body is tinted with the active layer's colour (base layer stays ginger)
 *
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>
#include <zmk/split/central.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/battery_state_changed.h>
#if IS_ENABLED(CONFIG_ZMK_SPLIT_CHARGING)
#include <zmk/events/split_charging_state_changed.h>
#endif

#include "bongo_cat.h"
#include "bongo_cat_frames.h"
#include "../layer_colors.h"

#define PERIPHERALS ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT

enum bongo_paws
{
    PAWS_IDLE,
    PAWS_LEFT,
    PAWS_RIGHT,
};

// Updated from event context, read when rendering on the display queue
static struct
{
    enum bongo_paws paws;
    bool sleeping;
    bool fast;
    uint8_t layer;
    int8_t level[PERIPHERALS];
    bool charging[PERIPHERALS];
} cat;

// Only touched from the event manager context
static int keys_down;
static bool next_left = true;

static struct zmk_widget_bongo_cat *bongo;

// RAM copy of the current base frame so its body colours can be retinted
static uint8_t frame_buf[BONGO_FRAME_DATA_SIZE];
static lv_img_dsc_t frame_dsc;

static void tint_palette(uint8_t *data, uint8_t index, uint32_t rgb)
{
    uint8_t *entry = &data[index * 4]; // lv_color32_t: B, G, R, A
    entry[0] = rgb & 0xFF;
    entry[1] = (rgb >> 8) & 0xFF;
    entry[2] = (rgb >> 16) & 0xFF;
}

static uint32_t darken(uint32_t rgb)
{
    uint32_t r = ((rgb >> 16) & 0xFF) * 3 / 4;
    uint32_t g = ((rgb >> 8) & 0xFF) * 3 / 4;
    uint32_t b = (rgb & 0xFF) * 3 / 4;
    return (r << 16) | (g << 8) | b;
}

static bool battery_low(void)
{
    for (int i = 0; i < PERIPHERALS; i++)
    {
        if (cat.level[i] >= 1 && cat.level[i] < CONFIG_DONGLE_SCREEN_LOW_BATTERY_PCT && !cat.charging[i])
        {
            return true;
        }
    }
    return false;
}

static void set_overlay(lv_obj_t *obj, const lv_img_dsc_t *src, lv_coord_t x, lv_coord_t y)
{
    if (lv_img_get_src(obj) != src)
    {
        lv_img_set_src(obj, src);
        lv_obj_set_pos(obj, x, y);
    }
}

static void set_visible(lv_obj_t *obj, bool visible)
{
    if (visible)
    {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static void render_work_cb(struct k_work *work)
{
    static const lv_img_dsc_t *shown_frame;
    static int shown_layer = -1;

    if (!bongo)
    {
        return;
    }

    const lv_img_dsc_t *frame = cat.paws == PAWS_LEFT    ? &bongo_left
                                : cat.paws == PAWS_RIGHT ? &bongo_right
                                                         : &bongo_idle;
    uint8_t layer = cat.layer;

    if (frame != shown_frame || layer != shown_layer)
    {
        memcpy(frame_buf, frame->data, MIN(frame->data_size, sizeof(frame_buf)));
        if (layer != 0)
        {
            uint32_t color = layer_color(layer);
            tint_palette(frame_buf, BONGO_PALETTE_BODY, color);
            tint_palette(frame_buf, BONGO_PALETTE_OUTLINE, darken(color));
        }
        frame_dsc.header = frame->header;
        frame_dsc.data_size = MIN(frame->data_size, sizeof(frame_buf));
        frame_dsc.data = frame_buf;

        lv_img_cache_invalidate_src(&frame_dsc);
        lv_img_set_src(bongo->frame, &frame_dsc);
        lv_obj_invalidate(bongo->frame);

        shown_frame = frame;
        shown_layer = layer;
    }

    bool sleeping = cat.sleeping;
    if (sleeping)
    {
        set_overlay(bongo->eyes, &bongo_eyes_closed, BONGO_EYES_CLOSED_X, BONGO_EYES_CLOSED_Y);
    }
    else
    {
        set_overlay(bongo->eyes, &bongo_eyes_open, BONGO_EYES_OPEN_X, BONGO_EYES_OPEN_Y);
    }

    if (battery_low())
    {
        set_overlay(bongo->mouth, &bongo_mouth_frown, BONGO_MOUTH_FROWN_X, BONGO_MOUTH_FROWN_Y);
    }
    else
    {
        set_overlay(bongo->mouth, &bongo_mouth_smile, BONGO_MOUTH_SMILE_X, BONGO_MOUTH_SMILE_Y);
    }

    set_visible(bongo->zzz, sleeping);
    set_visible(bongo->sweat, cat.fast && !sleeping);
}

static K_WORK_DEFINE(render_work, render_work_cb);

static void request_render(void)
{
    if (bongo && zmk_display_is_initialized())
    {
        k_work_submit_to_queue(zmk_display_work_q(), &render_work);
    }
}

static void sleep_work_cb(struct k_work *work)
{
    cat.sleeping = true;
    request_render();
}

static K_WORK_DELAYABLE_DEFINE(sleep_work, sleep_work_cb);

static void on_position(const struct zmk_position_state_changed *ev)
{
    if (ev->state)
    {
        keys_down++;
        cat.paws = next_left ? PAWS_LEFT : PAWS_RIGHT;
        next_left = !next_left;
        cat.sleeping = false;
        k_work_reschedule(&sleep_work, K_SECONDS(CONFIG_DONGLE_SCREEN_BONGO_CAT_SLEEP_S));
    }
    else
    {
        if (keys_down > 0)
        {
            keys_down--;
        }
        if (keys_down == 0)
        {
            cat.paws = PAWS_IDLE;
        }
    }
}

static int bongo_cat_listener(const zmk_event_t *eh)
{
    const struct zmk_position_state_changed *pos;
    const struct zmk_wpm_state_changed *wpm;
    const struct zmk_peripheral_battery_state_changed *bat;

    if ((pos = as_zmk_position_state_changed(eh)))
    {
        on_position(pos);
    }
    else if ((wpm = as_zmk_wpm_state_changed(eh)))
    {
        cat.fast = wpm->state >= CONFIG_DONGLE_SCREEN_BONGO_CAT_FAST_WPM;
    }
    else if (as_zmk_layer_state_changed(eh))
    {
        cat.layer = zmk_keymap_highest_layer_active();
    }
    else if ((bat = as_zmk_peripheral_battery_state_changed(eh)))
    {
        if (bat->source < PERIPHERALS)
        {
            cat.level[bat->source] = bat->state_of_charge;
        }
    }
#if IS_ENABLED(CONFIG_ZMK_SPLIT_CHARGING)
    else
    {
        const struct zmk_split_charging_state_changed *chg = as_zmk_split_charging_state_changed(eh);
        if (chg && chg->source < PERIPHERALS)
        {
            cat.charging[chg->source] = chg->charging;
        }
    }
#endif

    request_render();
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(bongo_cat, bongo_cat_listener);
ZMK_SUBSCRIPTION(bongo_cat, zmk_position_state_changed);
ZMK_SUBSCRIPTION(bongo_cat, zmk_wpm_state_changed);
ZMK_SUBSCRIPTION(bongo_cat, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(bongo_cat, zmk_peripheral_battery_state_changed);
#if IS_ENABLED(CONFIG_ZMK_SPLIT_CHARGING)
ZMK_SUBSCRIPTION(bongo_cat, zmk_split_charging_state_changed);
#endif

static lv_obj_t *create_img(lv_obj_t *parent)
{
    lv_obj_t *img = lv_img_create(parent);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
    return img;
}

int zmk_widget_bongo_cat_init(struct zmk_widget_bongo_cat *widget, lv_obj_t *parent)
{
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, BONGO_W, BONGO_H);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(widget->obj, 0, 0);
    lv_obj_set_style_pad_all(widget->obj, 0, 0);
    lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_SCROLLABLE);

    widget->frame = create_img(widget->obj);
    widget->eyes = create_img(widget->obj);
    widget->mouth = create_img(widget->obj);

    widget->sweat = create_img(widget->obj);
    lv_img_set_src(widget->sweat, &bongo_sweat);
    lv_obj_set_pos(widget->sweat, BONGO_SWEAT_X, BONGO_SWEAT_Y);

    widget->zzz = create_img(widget->obj);
    lv_img_set_src(widget->zzz, &bongo_zzz);
    lv_obj_set_pos(widget->zzz, BONGO_ZZZ_X, BONGO_ZZZ_Y);

    for (int i = 0; i < PERIPHERALS; i++)
    {
        cat.level[i] = -1;
    }
    cat.layer = zmk_keymap_highest_layer_active();

    bongo = widget;
    // Draw the initial state directly: we're already on the display thread
    render_work_cb(NULL);
    k_work_schedule(&sleep_work, K_SECONDS(CONFIG_DONGLE_SCREEN_BONGO_CAT_SLEEP_S));
    return 0;
}

lv_obj_t *zmk_widget_bongo_cat_obj(struct zmk_widget_bongo_cat *widget)
{
    return widget->obj;
}
