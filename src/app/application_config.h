/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

namespace app {

struct ApplicationConfig {
    bool dark_mode = false;
    // Last-used jiggler settings. has_* is false when the key is absent, so the
    // app keeps its built-in defaults (mode=mouse, interval 5-15 s).
    bool has_mode = false;
    int mode = 0; // 0 mouse, 1 keyboard, 2 both (model::JigglerMode)
    bool has_interval_min = false;
    int interval_min_seconds = 5;
};

bool load_application_config(const std::string& path,
                             ApplicationConfig& config,
                             std::string& error);
bool save_application_config(const std::string& path,
                             const ApplicationConfig& config,
                             std::string& error);

} // namespace app
