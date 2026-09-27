/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <zephyr/sys/util.h>

// Neon colour per layer index (layer name, frame, chips); layers past the end reuse the last one
static const uint32_t layer_colors[] = {
    0xFF2A6D, // 0 BASE: magenta
    0x00F0FF, // 1 NAV: cyan
    0xFCEE0A, // 2 SYM: yellow
    0xFF1E32, // 3 ADJUST: red
    0x39FF88, // 4+ NUM: green
};

static inline uint32_t layer_color(uint8_t index)
{
    return layer_colors[MIN(index, ARRAY_SIZE(layer_colors) - 1)];
}
