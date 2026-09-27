/*
 * Screen frame: grid behind the middle band, corner brackets and the notched
 * divider above the batteries. Brackets and divider follow the layer colour.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>
#include <zmk/events/layer_state_changed.h>

#include "frame_decor.h"
#include "../layer_colors.h"
#include "../theme.h"

#define GRID_TOP 84
#define GRID_BOTTOM 196
#define GRID_ROW 16
#define GRID_COL 20
#define GRID_ROWS ((GRID_BOTTOM - GRID_TOP) / GRID_ROW + 1)
#define GRID_COLS_MAX 16
#define BRACKET 10
#define DIVIDER_Y 200
#define NOTCH_W 34
#define NOTCH_H 6

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

// lv_line keeps a pointer to its points, so they live here
static lv_point_t grid_h[GRID_ROWS * 2];
static lv_point_t grid_v[GRID_COLS_MAX * 2];
static lv_point_t bracket_pts[4][3];
static lv_point_t divider_pts[6];

static lv_obj_t *line(lv_obj_t *parent, const lv_point_t *pts, uint16_t n, uint32_t color)
{
    lv_obj_t *l = lv_line_create(parent);
    lv_line_set_points(l, pts, n);
    lv_obj_set_style_line_width(l, 1, 0);
    lv_obj_set_style_line_color(l, CP_COLOR(color), 0);
    lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
    return l;
}

struct frame_decor_state
{
    uint8_t layer;
};

static struct frame_decor_state get_state(const zmk_event_t *eh)
{
    return (struct frame_decor_state){.layer = zmk_keymap_highest_layer_active()};
}

static void frame_decor_update_cb(struct frame_decor_state state)
{
    lv_color_t tint = CP_COLOR(layer_color(state.layer));
    struct zmk_widget_frame_decor *widget;

    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node)
    {
        for (int i = 0; i < ARRAY_SIZE(widget->brackets); i++)
        {
            lv_obj_set_style_line_color(widget->brackets[i], tint, 0);
        }
        lv_obj_set_style_line_color(widget->divider, tint, 0);
        lv_obj_set_style_text_color(widget->net, tint, 0);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_frame_decor, struct frame_decor_state, frame_decor_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_frame_decor, zmk_layer_state_changed);

int zmk_widget_frame_decor_init(struct zmk_widget_frame_decor *widget, lv_obj_t *parent)
{
    const lv_coord_t w = lv_disp_get_hor_res(NULL), h = lv_disp_get_ver_res(NULL);

    // Grid as two serpentine polylines; the turns land on the outer grid lines
    for (int r = 0; r < GRID_ROWS; r++)
    {
        lv_coord_t y = GRID_TOP + r * GRID_ROW;
        grid_h[r * 2] = (lv_point_t){r % 2 ? w - 1 : 0, y};
        grid_h[r * 2 + 1] = (lv_point_t){r % 2 ? 0 : w - 1, y};
    }
    int cols = MIN((w - 1) / GRID_COL + 1, GRID_COLS_MAX);
    for (int c = 0; c < cols; c++)
    {
        lv_coord_t x = c * GRID_COL;
        grid_v[c * 2] = (lv_point_t){x, c % 2 ? GRID_BOTTOM : GRID_TOP};
        grid_v[c * 2 + 1] = (lv_point_t){x, c % 2 ? GRID_TOP : GRID_BOTTOM};
    }
    line(parent, grid_h, GRID_ROWS * 2, CP_GRID);
    line(parent, grid_v, cols * 2, CP_GRID);

    // Corner brackets: {corner x, corner y, x direction, y direction}
    const lv_coord_t corners[4][4] = {{1, 1, 1, 1}, {w - 2, 1, -1, 1}, {1, h - 2, 1, -1}, {w - 2, h - 2, -1, -1}};
    for (int i = 0; i < 4; i++)
    {
        lv_coord_t x = corners[i][0], y = corners[i][1];
        bracket_pts[i][0] = (lv_point_t){x + BRACKET * corners[i][2], y};
        bracket_pts[i][1] = (lv_point_t){x, y};
        bracket_pts[i][2] = (lv_point_t){x, y + BRACKET * corners[i][3]};
        widget->brackets[i] = line(parent, bracket_pts[i], 3, CP_MAGENTA);
    }

    // Divider with a raised notch in the middle
    lv_coord_t mid = w / 2;
    divider_pts[0] = (lv_point_t){0, DIVIDER_Y};
    divider_pts[1] = (lv_point_t){mid - NOTCH_W / 2 - NOTCH_H, DIVIDER_Y};
    divider_pts[2] = (lv_point_t){mid - NOTCH_W / 2, DIVIDER_Y - NOTCH_H};
    divider_pts[3] = (lv_point_t){mid + NOTCH_W / 2, DIVIDER_Y - NOTCH_H};
    divider_pts[4] = (lv_point_t){mid + NOTCH_W / 2 + NOTCH_H, DIVIDER_Y};
    divider_pts[5] = (lv_point_t){w - 1, DIVIDER_Y};
    widget->divider = line(parent, divider_pts, ARRAY_SIZE(divider_pts), CP_MAGENTA);

    widget->net = cp_label(parent, &cp_mono_10, CP_MAGENTA, "NET");
    lv_obj_align(widget->net, LV_ALIGN_TOP_MID, 0, DIVIDER_Y - NOTCH_H - 12);

    sys_slist_append(&widgets, &widget->node);
    widget_frame_decor_init();
    return 0;
}
