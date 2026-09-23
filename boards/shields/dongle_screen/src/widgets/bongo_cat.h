/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_bongo_cat
{
    lv_obj_t *obj;
    lv_obj_t *frame;
    lv_obj_t *eyes;
    lv_obj_t *mouth;
    lv_obj_t *sweat;
    lv_obj_t *zzz;
};

int zmk_widget_bongo_cat_init(struct zmk_widget_bongo_cat *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_bongo_cat_obj(struct zmk_widget_bongo_cat *widget);
