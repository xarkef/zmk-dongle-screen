/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_frame_decor
{
    sys_snode_t node;
    lv_obj_t *divider;
    lv_obj_t *net;
};

// Call first so the grid sits behind every other widget
int zmk_widget_frame_decor_init(struct zmk_widget_frame_decor *widget, lv_obj_t *parent);
