/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

#define DATA_STREAM_ROWS 5

struct zmk_widget_data_stream
{
    lv_obj_t *obj;
    lv_obj_t *rows[DATA_STREAM_ROWS];
    lv_obj_t *marker_left;
    lv_obj_t *marker_right;
    lv_obj_t *status;
};

int zmk_widget_data_stream_init(struct zmk_widget_data_stream *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_data_stream_obj(struct zmk_widget_data_stream *widget);
