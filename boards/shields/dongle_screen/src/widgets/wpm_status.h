/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

#define WPM_BARS 8

struct zmk_widget_wpm_status
{
    lv_obj_t *obj;
    lv_obj_t *wpm_label;
    lv_obj_t *bars[WPM_BARS];
    sys_snode_t node;
};

int zmk_widget_wpm_status_init(struct zmk_widget_wpm_status *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_wpm_status_obj(struct zmk_widget_wpm_status *widget);
