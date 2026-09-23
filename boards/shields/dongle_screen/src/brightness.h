/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>

/**
 * @brief Wake the screen when a peripheral reconnects
 * Called by battery widget when it detects a peripheral reconnection
 */
void brightness_wake_screen_on_reconnect(void);
/**
 * @brief Wake the screen on a touch if it went dark from idle
 * @return true if the screen was woken (the touch should be swallowed)
 */
bool brightness_wake_screen_on_touch(void);
