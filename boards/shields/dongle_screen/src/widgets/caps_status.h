/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_caps_status
{
    lv_obj_t *obj;
    sys_snode_t node;
};

int zmk_widget_caps_status_init(struct zmk_widget_caps_status *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_caps_status_obj(struct zmk_widget_caps_status *widget);
