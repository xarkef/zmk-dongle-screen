/*
 * Cyberpunk theme: palette, fonts and small drawing helpers shared by the widgets.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <fonts.h>

#define CP_BG 0x000000
#define CP_GRID 0x141028
#define CP_DIM 0x463C6E
#define CP_TEXT 0xEBEBF5
#define CP_CYAN 0x00F0FF
#define CP_MAGENTA 0xFF2A6D
#define CP_YELLOW 0xFCEE0A
#define CP_RED 0xFF1E32
#define CP_GREEN 0x39FF88

#define CP_COLOR(hex) lv_color_hex(hex)

// Transparent, borderless, unpadded container at (x, y)
lv_obj_t *cp_container(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h);

// Label with a font and colour
lv_obj_t *cp_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, const char *text);

// Box with the top-left and bottom-right corners clipped, drawn on a canvas, with a centred label
struct cp_chip
{
    lv_obj_t *canvas;
    lv_obj_t *label;
    lv_coord_t w, h, cut;
    uint32_t color;
    bool filled;
};

// buf must hold w * h lv_color_t and outlive the chip
void cp_chip_init(struct cp_chip *chip, lv_obj_t *parent, lv_color_t *buf, lv_coord_t x, lv_coord_t y,
                  lv_coord_t w, lv_coord_t h, lv_coord_t cut, const lv_font_t *font, const char *text);
// Outlined: label in the chip colour. Filled: label in the background colour.
void cp_chip_set(struct cp_chip *chip, uint32_t color, bool filled);
