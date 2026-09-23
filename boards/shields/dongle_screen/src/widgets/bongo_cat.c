/*
 * Bongo cat: slaps the desk with alternating paws on every key press,
 * returns to idle once all keys are released.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>

#include "bongo_cat.h"

extern const lv_img_dsc_t bongo_idle;
extern const lv_img_dsc_t bongo_left;
extern const lv_img_dsc_t bongo_right;

enum bongo_frame
{
    BONGO_IDLE,
    BONGO_LEFT,
    BONGO_RIGHT,
};

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct bongo_cat_state
{
    enum bongo_frame frame;
};

// Only touched from the event manager context
static int keys_down;
static bool next_left = true;
static enum bongo_frame current = BONGO_IDLE;

static struct bongo_cat_state get_state(const zmk_event_t *eh)
{
    const struct zmk_position_state_changed *ev = eh ? as_zmk_position_state_changed(eh) : NULL;

    if (ev)
    {
        if (ev->state)
        {
            keys_down++;
            current = next_left ? BONGO_LEFT : BONGO_RIGHT;
            next_left = !next_left;
        }
        else
        {
            if (keys_down > 0)
            {
                keys_down--;
            }
            if (keys_down == 0)
            {
                current = BONGO_IDLE;
            }
        }
    }

    return (struct bongo_cat_state){.frame = current};
}

static void set_frame(struct zmk_widget_bongo_cat *widget, struct bongo_cat_state state)
{
    switch (state.frame)
    {
    case BONGO_LEFT:
        lv_img_set_src(widget->obj, &bongo_left);
        break;
    case BONGO_RIGHT:
        lv_img_set_src(widget->obj, &bongo_right);
        break;
    default:
        lv_img_set_src(widget->obj, &bongo_idle);
        break;
    }
}

static void bongo_cat_update_cb(struct bongo_cat_state state)
{
    struct zmk_widget_bongo_cat *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node)
    {
        set_frame(widget, state);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_bongo_cat, struct bongo_cat_state,
                            bongo_cat_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_bongo_cat, zmk_position_state_changed);

int zmk_widget_bongo_cat_init(struct zmk_widget_bongo_cat *widget, lv_obj_t *parent)
{
    widget->obj = lv_img_create(parent);
    lv_img_set_src(widget->obj, &bongo_idle);

    sys_slist_append(&widgets, &widget->node);

    widget_bongo_cat_init();
    return 0;
}

lv_obj_t *zmk_widget_bongo_cat_obj(struct zmk_widget_bongo_cat *widget)
{
    return widget->obj;
}
