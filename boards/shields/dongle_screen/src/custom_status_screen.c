/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "custom_status_screen.h"

#if CONFIG_DONGLE_SCREEN_OUTPUT_ACTIVE
#include "widgets/output_status.h"
static struct zmk_widget_output_status output_status_widget;
#endif

#if CONFIG_DONGLE_SCREEN_LAYER_ACTIVE
#include "widgets/layer_status.h"
static struct zmk_widget_layer_status layer_status_widget;
#endif

#if CONFIG_DONGLE_SCREEN_BATTERY_ACTIVE
#include "widgets/battery_status.h"
static struct zmk_widget_dongle_battery_status dongle_battery_status_widget;
#endif

#if CONFIG_DONGLE_SCREEN_WPM_ACTIVE
#include "widgets/wpm_status.h"
static struct zmk_widget_wpm_status wpm_status_widget;
#endif

#if CONFIG_DONGLE_SCREEN_MODIFIER_ACTIVE
#include "widgets/mod_status.h"
static struct zmk_widget_mod_status mod_widget;
#endif

#if CONFIG_DONGLE_SCREEN_BONGO_CAT_ACTIVE
#include "widgets/bongo_cat.h"
static struct zmk_widget_bongo_cat bongo_cat_widget;
#endif

#if CONFIG_DONGLE_SCREEN_CAPS_ACTIVE
#include "widgets/caps_status.h"
static struct zmk_widget_caps_status caps_status_widget;
#endif

#if CONFIG_DONGLE_SCREEN_DATA_STREAM_ACTIVE
#include "widgets/data_stream.h"
static struct zmk_widget_data_stream data_stream_widget;
#endif

#include "widgets/frame_decor.h"
static struct zmk_widget_frame_decor frame_decor_widget;

#include "theme.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

lv_style_t global_style;

// Top-left position of a widget, centred horizontally when x < 0
static void place(lv_obj_t *obj, lv_coord_t x, lv_coord_t y)
{
    if (x < 0)
    {
        lv_obj_update_layout(obj);
        x = (lv_disp_get_hor_res(NULL) - lv_obj_get_width(obj)) / 2;
    }
    lv_obj_set_pos(obj, x, y);
}

lv_obj_t *zmk_display_status_screen()
{
    lv_obj_t *screen;
    const lv_coord_t w = lv_disp_get_hor_res(NULL);

    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, CP_COLOR(CP_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, 255, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_style_init(&global_style);
    lv_style_set_text_font(&global_style, &cp_mono_12);
    lv_style_set_text_color(&global_style, CP_COLOR(CP_TEXT));
    lv_style_set_text_letter_space(&global_style, 0);
    lv_style_set_text_line_space(&global_style, 1);
    lv_obj_add_style(screen, &global_style, LV_PART_MAIN);

    // First, so the grid sits behind everything
    zmk_widget_frame_decor_init(&frame_decor_widget, screen);

#if CONFIG_DONGLE_SCREEN_WPM_ACTIVE
    zmk_widget_wpm_status_init(&wpm_status_widget, screen);
    place(zmk_widget_wpm_status_obj(&wpm_status_widget), 10, 8);
#endif

#if CONFIG_DONGLE_SCREEN_OUTPUT_ACTIVE
    zmk_widget_output_status_init(&output_status_widget, screen);
    place(zmk_widget_output_status_obj(&output_status_widget), w - 10 - 44, 8);
#endif

#if CONFIG_DONGLE_SCREEN_CAPS_ACTIVE
    zmk_widget_caps_status_init(&caps_status_widget, screen);
    place(zmk_widget_caps_status_obj(&caps_status_widget), 10, 64);
#endif

#if CONFIG_DONGLE_SCREEN_DATA_STREAM_ACTIVE
    zmk_widget_data_stream_init(&data_stream_widget, screen);
    place(zmk_widget_data_stream_obj(&data_stream_widget), -1, 6);
#endif

#if CONFIG_DONGLE_SCREEN_BONGO_CAT_ACTIVE
    zmk_widget_bongo_cat_init(&bongo_cat_widget, screen);
    lv_obj_align(zmk_widget_bongo_cat_obj(&bongo_cat_widget), LV_ALIGN_TOP_MID, 0, 8);
#endif

#if CONFIG_DONGLE_SCREEN_LAYER_ACTIVE
    zmk_widget_layer_status_init(&layer_status_widget, screen);
    place(zmk_widget_layer_status_obj(&layer_status_widget), 0, 92);
#endif

#if CONFIG_DONGLE_SCREEN_MODIFIER_ACTIVE
    zmk_widget_mod_status_init(&mod_widget, screen);
    place(zmk_widget_mod_status_obj(&mod_widget), -1, 150);
#endif

#if CONFIG_DONGLE_SCREEN_BATTERY_ACTIVE
    zmk_widget_dongle_battery_status_init(&dongle_battery_status_widget, screen);
    place(zmk_widget_dongle_battery_status_obj(&dongle_battery_status_widget), -1, 205);
#endif

    return screen;
}
