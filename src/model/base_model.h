/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

namespace model {

// Which HID device the jiggler pretends to be when it fires.
enum class JigglerMode {
    Mouse    = 0,
    Keyboard = 1,
    Both     = 2,
};

const char* jiggler_mode_label(JigglerMode mode);
JigglerMode next_jiggler_mode(JigglerMode mode);

class BaseModel {
public:
    const char* app_title() const;

    bool dark_mode() const;
    void set_dark_mode(bool enabled);
    void toggle_dark_mode();

    bool is_jiggling() const;
    void set_jiggling(bool active);

    JigglerMode jiggler_mode() const;
    void cycle_jiggler_mode();

    int interval_min_seconds() const;
    int interval_max_seconds() const;
    void increase_interval();
    void decrease_interval();

    int moves_sent() const;
    void increment_moves_sent();
    void reset_moves_sent();

    bool info_visible() const;
    void toggle_info();

private:
    bool dark_mode_ = false;
    bool jiggling_active_ = false;
    JigglerMode jiggler_mode_ = JigglerMode::Both;
    int interval_min_seconds_ = 5;
    int interval_max_seconds_ = 15;
    int moves_sent_ = 0;
    bool info_visible_ = false;
};

} // namespace model
