/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>
#include "../theme.h"

struct zmk_widget_output_status
{
    lv_obj_t *obj;
    struct cp_chip transport;
    lv_obj_t *ble_label;
    sys_snode_t node;
};

int zmk_widget_output_status_init(struct zmk_widget_output_status *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_output_status_obj(struct zmk_widget_output_status *widget);
