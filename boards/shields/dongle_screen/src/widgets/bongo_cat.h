/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_bongo_cat
{
    lv_obj_t *obj;
    sys_snode_t node;
};

int zmk_widget_bongo_cat_init(struct zmk_widget_bongo_cat *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_bongo_cat_obj(struct zmk_widget_bongo_cat *widget);
