/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <zephyr/sys/util.h>

// Colour per layer index (layer name + cat tint); layers past the end reuse the last one
static const uint32_t layer_colors[] = {
    0xFFFFFF, // 0 base: white (cat stays ginger)
    0x4FC3F7, // 1 lower: sky blue
    0xFFB74D, // 2 raise: amber
    0xF06292, // 3 adjust: pink
    0xAED581, // 4+: lime
};

static inline uint32_t layer_color(uint8_t index)
{
    return layer_colors[MIN(index, ARRAY_SIZE(layer_colors) - 1)];
}
