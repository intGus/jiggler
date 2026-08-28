/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "navbar.h"

#include "asset_manager.h"
#include "bindings.h"
#include "linux_input.h"
#include "theme.h"
#include "ui_const.h"

namespace view::widgets {
namespace {

constexpr std::array<int32_t, 5> kNavButtonXOffsets = {30, 17, 2, -15, -30};

} // namespace

NavBar::NavBar(lv_obj_t* parent, viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseWidgets(parent), view_model_(view_model), assets_(assets) {}

NavBar::~NavBar() {
    for (size_t i = 0; i < icon_buttons_.size(); ++i) {
        if (icon_buttons_[i]) {
            platform::unregister_nav_button(i, icon_buttons_[i]->root());
        }
    }

    if (active_observer_) {
        lv_observer_remove(active_observer_);
        active_observer_ = nullptr;
    }
}

void NavBar::build() {
    if (core_obj_) {
        return;
    }

    core_obj_ = lv_obj_create(parent_);
    lv_obj_remove_style_all(core_obj_);
    lv_obj_set_size(core_obj_, LV_PCT(100), kNavBarHeight);
    lv_obj_align(core_obj_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(core_obj_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(core_obj_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(core_obj_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(core_obj_, 8, 0);
    lv_obj_set_style_pad_right(core_obj_, 8, 0);
    reactive::bind_theme(core_obj_, view_model_.dark_mode_subject(), reactive::ThemeRole::Bar);

    create_icon_buttons();
    active_observer_ = reactive::observe_obj(core_obj_, view_model_.active_subject(), active_changed_cb, this);
    update_icon_buttons();
}

void NavBar::create_icon_buttons() {
    const auto light_color = view::palette(false).text;
    const auto dark_color = view::palette(true).text;
    icon_font_ = assets_.load_font("Phosphor-Fill.ttf", 22);
    // Deliberately not routed through assets_.load_standard_font(): that
    // resolves to a device-only Noto Sans CJK file and always falls back on
    // desktop builds, so its fallback size (14px) does not reliably fit
    // 4-letter labels ("STOP", "MODE") in a 32px-wide nav button. This
    // built-in bitmap font is guaranteed present on both targets and small
    // enough to fit reliably.
    label_font_ = &lv_font_montserrat_12;

    struct ButtonSpec {
        const char* text;
        lv_event_cb_t click_cb;
        bool use_icon_font;
    };
    const std::array<ButtonSpec, 5> specs = {{
        {"RUN", toggle_jiggling_cb, false},
        {"MODE", cycle_mode_cb, false},
        {ICON_MINUS, decrease_interval_cb, true},
        {ICON_PLUS, increase_interval_cb, true},
        {ICON_INFO, toggle_info_cb, true},
    }};

    for (size_t i = 0; i < icon_buttons_.size(); ++i) {
        const auto& spec = specs[i];
        const lv_font_t* font = spec.use_icon_font ? icon_font_ : label_font_;
        icon_buttons_[i] = std::make_unique<IconButton>(core_obj_,
                                                        view_model_,
                                                        32,
                                                        24,
                                                        spec.text,
                                                        font ? font : &lv_font_montserrat_14,
                                                        light_color,
                                                        dark_color,
                                                        spec.click_cb,
                                                        &view_model_,
                                                        true);
        icon_buttons_[i]->build();
        lv_obj_set_style_translate_x(icon_buttons_[i]->root(), kNavButtonXOffsets[i], 0);
        platform::register_nav_button(i, icon_buttons_[i]->root());
    }
}

void NavBar::update_icon_buttons() {
    if (!icon_buttons_[0]) {
        return;
    }
    icon_buttons_[0]->set_text(view_model_.is_jiggling() ? "STOP" : "RUN");
    icon_buttons_[0]->set_font(label_font_ ? label_font_ : &lv_font_montserrat_14);
}

void NavBar::toggle_jiggling_cb(lv_event_t* event) {
    auto* view_model = static_cast<viewmodel::BaseViewModel*>(lv_event_get_user_data(event));
    if (view_model) {
        view_model->toggle_jiggling();
    }
}

void NavBar::cycle_mode_cb(lv_event_t* event) {
    auto* view_model = static_cast<viewmodel::BaseViewModel*>(lv_event_get_user_data(event));
    if (view_model) {
        view_model->cycle_jiggler_mode();
    }
}

void NavBar::decrease_interval_cb(lv_event_t* event) {
    auto* view_model = static_cast<viewmodel::BaseViewModel*>(lv_event_get_user_data(event));
    if (view_model) {
        view_model->decrease_interval();
    }
}

void NavBar::increase_interval_cb(lv_event_t* event) {
    auto* view_model = static_cast<viewmodel::BaseViewModel*>(lv_event_get_user_data(event));
    if (view_model) {
        view_model->increase_interval();
    }
}

void NavBar::toggle_info_cb(lv_event_t* event) {
    auto* view_model = static_cast<viewmodel::BaseViewModel*>(lv_event_get_user_data(event));
    if (view_model) {
        view_model->toggle_info();
    }
}

void NavBar::active_changed_cb(lv_observer_t* observer, lv_subject_t* subject) {
    LV_UNUSED(subject);

    auto* nav_bar = static_cast<NavBar*>(lv_observer_get_user_data(observer));
    if (nav_bar) {
        nav_bar->update_icon_buttons();
    }
}

} // namespace view::widgets
