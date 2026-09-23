/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/kernel.h>
#include <zmk/event_manager.h>

// Raised on the central when a peripheral (half) gains/loses USB power.
// `source` is the same peripheral index as zmk_peripheral_battery_state_changed.
struct zmk_split_charging_state_changed
{
    uint8_t source;
    bool charging;
};

ZMK_EVENT_DECLARE(zmk_split_charging_state_changed);
