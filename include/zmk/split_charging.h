/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/bluetooth/uuid.h>

#define ZMK_SPLIT_CHARGING_UUID(num) BT_UUID_128_ENCODE(num, 0x5c3e, 0x4a8d, 0x9e21, 0x6a7c3b1d0e01)
#define ZMK_SPLIT_CHARGING_SERVICE_UUID ZMK_SPLIT_CHARGING_UUID(0x2f6b6a10)
#define ZMK_SPLIT_CHARGING_CHAR_UUID ZMK_SPLIT_CHARGING_UUID(0x2f6b6a11)
