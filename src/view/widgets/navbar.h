/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_widget.h"
#include "base_viewmodel.h"
#include "icon_button.h"

#include <array>
#include <memory>

namespace app {
class AssetManager;
}

namespace view::widgets {

// Fixed 5-button bar mapped to hardware/keyboard keys 4-8 (left to right):
// RUN/STOP, Mode, -Interval, +Interval, Info. There is only one screen in
// this app, so unlike the template this bar does not change per page.
class NavBar : public BaseWidgets {
public:
    NavBar(lv_obj_t* parent, viewmodel::BaseViewModel& view_model, app::AssetManager& assets);
    ~NavBar() override;

    void build() override;

private:
    void create_icon_buttons();
    void update_icon_buttons();

    static void toggle_jiggling_cb(lv_event_t* event);
    static void cycle_mode_cb(lv_event_t* event);
    static void decrease_interval_cb(lv_event_t* event);
    static void increase_interval_cb(lv_event_t* event);
    static void toggle_info_cb(lv_event_t* event);
    static void active_changed_cb(lv_observer_t* observer, lv_subject_t* subject);

    viewmodel::BaseViewModel& view_model_;
    app::AssetManager& assets_;
    std::array<std::unique_ptr<IconButton>, 5> icon_buttons_;
    lv_font_t* icon_font_{nullptr};
    const lv_font_t* label_font_{nullptr};
    lv_observer_t* active_observer_{nullptr};
};

} // namespace view::widgets
