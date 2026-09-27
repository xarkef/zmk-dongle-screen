/*
 * SPDX-License-Identifier: MIT
 */

#include "theme.h"

lv_obj_t *cp_container(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

lv_obj_t *cp_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, CP_COLOR(color), 0);
    lv_obj_set_style_text_letter_space(label, 0, 0);
    lv_label_set_text(label, text);
    return label;
}

void cp_chip_init(struct cp_chip *chip, lv_obj_t *parent, lv_color_t *buf, lv_coord_t x, lv_coord_t y,
                  lv_coord_t w, lv_coord_t h, lv_coord_t cut, const lv_font_t *font, const char *text)
{
    chip->w = w;
    chip->h = h;
    chip->cut = cut;
    chip->color = 0;
    chip->filled = false;

    chip->canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(chip->canvas, buf, w, h, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(chip->canvas, x, y);

    chip->label = cp_label(parent, font, CP_DIM, text);
    lv_obj_align_to(chip->label, chip->canvas, LV_ALIGN_CENTER, 0, 0);
}

void cp_chip_set(struct cp_chip *chip, uint32_t color, bool filled)
{
    if (chip->color == color && chip->filled == filled)
    {
        return;
    }
    chip->color = color;
    chip->filled = filled;

    lv_coord_t w = chip->w - 1, h = chip->h - 1, c = chip->cut;
    lv_point_t pts[] = {{c, 0}, {w, 0}, {w, h - c}, {w - c, h}, {0, h}, {0, c}, {c, 0}};

    lv_canvas_fill_bg(chip->canvas, CP_COLOR(CP_BG), LV_OPA_COVER);
    if (filled)
    {
        lv_draw_rect_dsc_t rect;
        lv_draw_rect_dsc_init(&rect);
        rect.bg_color = CP_COLOR(color);
        rect.bg_opa = LV_OPA_COVER;
        lv_canvas_draw_polygon(chip->canvas, pts, ARRAY_SIZE(pts) - 1, &rect);
    }
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = CP_COLOR(color);
    line.width = 1;
    lv_canvas_draw_line(chip->canvas, pts, ARRAY_SIZE(pts), &line);

    lv_obj_set_style_text_color(chip->label, CP_COLOR(filled ? CP_BG : color), 0);
}
