/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "jiggler_screen.h"

#include "asset_manager.h"
#include "base_model.h"
#include "bindings.h"
#include "theme.h"

namespace screen {
namespace {

void mode_label_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* label = lv_observer_get_target_obj(observer);
    if (!label) {
        return;
    }
    const auto mode = static_cast<model::JigglerMode>(lv_subject_get_int(subject));
    lv_label_set_text_fmt(label, "Mode: %s", model::jiggler_mode_label(mode));
}

void interval_label_observer(lv_observer_t* observer, lv_subject_t* subject) {
    LV_UNUSED(subject);
    auto* label = lv_observer_get_target_obj(observer);
    auto* view_model = static_cast<viewmodel::BaseViewModel*>(lv_observer_get_user_data(observer));
    if (!label || !view_model) {
        return;
    }
    lv_label_set_text_fmt(label,
                          "Interval: %ds - %ds",
                          view_model->interval_min_seconds(),
                          view_model->interval_max_seconds());
}

void info_visible_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* panel = lv_observer_get_target_obj(observer);
    if (!panel) {
        return;
    }
    if (lv_subject_get_int(subject)) {
        lv_obj_move_foreground(panel);
        lv_obj_remove_flag(panel, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
    }
}

} // namespace

JigglerScreen::JigglerScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void JigglerScreen::build_content(lv_obj_t* content) {
    auto* group = lv_obj_create(content);
    lv_obj_remove_style_all(group);
    lv_obj_set_size(group, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(group, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(group, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(group, 3, 0);
    lv_obj_clear_flag(group, LV_OBJ_FLAG_SCROLLABLE);

    auto* status_font = assets().load_standard_font(20);
    auto* detail_font = assets().load_standard_font(12, app::StandardFontWeight::Regular);
    auto* small_font = assets().load_standard_font(11, app::StandardFontWeight::Regular);

    auto* status_label = lv_label_create(group);
    reactive::bind_bool_text(status_label, view_model().active_subject(), "IDLE", "JIGGLING");
    lv_obj_set_style_text_font(status_label, status_font ? status_font : &lv_font_montserrat_20, 0);
    reactive::bind_theme(status_label, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    auto* mode_label = lv_label_create(group);
    lv_label_set_text_fmt(mode_label, "Mode: %s", model::jiggler_mode_label(view_model().jiggler_mode()));
    lv_obj_set_style_text_font(mode_label, detail_font ? detail_font : &lv_font_montserrat_12, 0);
    lv_subject_add_observer_obj(view_model().mode_subject(), mode_label_observer, mode_label, nullptr);
    reactive::bind_theme(mode_label, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    auto* interval_label = lv_label_create(group);
    lv_label_set_text_fmt(interval_label,
                          "Interval: %ds - %ds",
                          view_model().interval_min_seconds(),
                          view_model().interval_max_seconds());
    lv_obj_set_style_text_font(interval_label, detail_font ? detail_font : &lv_font_montserrat_12, 0);
    lv_subject_add_observer_obj(view_model().interval_min_subject(),
                                interval_label_observer,
                                interval_label,
                                &view_model());
    lv_subject_add_observer_obj(view_model().interval_max_subject(),
                                interval_label_observer,
                                interval_label,
                                &view_model());
    reactive::bind_theme(interval_label, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    auto* status_detail_label = lv_label_create(group);
    lv_label_bind_text(status_detail_label, view_model().status_subject(), nullptr);
    lv_obj_set_style_text_font(status_detail_label, small_font ? small_font : &lv_font_montserrat_12, 0);
    reactive::bind_theme(status_detail_label, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    auto* moves_label = lv_label_create(group);
    lv_label_bind_text(moves_label, view_model().moves_sent_subject(), "Moves sent: %d");
    lv_obj_set_style_text_font(moves_label, small_font ? small_font : &lv_font_montserrat_12, 0);
    reactive::bind_theme(moves_label, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    // Info panel: covers the content area when toggled via the INFO nav
    // button (key 8). Explains what "jiggling" actually does on this build
    // so USB HID output is never assumed to be real hardware behavior.
    auto* info_panel = lv_obj_create(content);
    lv_obj_remove_style_all(info_panel);
    lv_obj_set_size(info_panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(info_panel, 8, 0);
    lv_obj_set_style_border_width(info_panel, 1, 0);
    lv_obj_clear_flag(info_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(info_panel, LV_OBJ_FLAG_HIDDEN);
    reactive::bind_theme(info_panel, view_model().dark_mode_subject(), reactive::ThemeRole::Surface);

    auto* info_title = lv_label_create(info_panel);
    lv_label_set_text(info_title, "About this build");
    lv_obj_set_style_text_font(info_title, detail_font ? detail_font : &lv_font_montserrat_12, 0);
    lv_obj_align(info_title, LV_ALIGN_TOP_LEFT, 0, 0);
    reactive::bind_theme(info_title, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    auto* info_body = lv_label_create(info_panel);
#if USE_DESKTOP
    lv_label_set_text(info_body,
                      "Desktop emulator: jiggling is simulated only.\n"
                      "On device: writes /dev/hidg0 (keyboard) and\n"
                      "/dev/hidg1 (mouse). Needs USB Slave/Device mode\n"
                      "with a HID gadget configured. Unverified on\n"
                      "real CardputerZero hardware.");
#else
    lv_label_set_text(info_body,
                      "Writes /dev/hidg0 (keyboard) and /dev/hidg1\n"
                      "(mouse). Needs USB Slave/Device mode with a\n"
                      "HID gadget already configured by the OS.\n"
                      "Unverified on real CardputerZero hardware.");
#endif
    lv_label_set_long_mode(info_body, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(info_body, LV_PCT(100));
    lv_obj_set_style_text_font(info_body, small_font ? small_font : &lv_font_montserrat_12, 0);
    lv_obj_align(info_body, LV_ALIGN_TOP_LEFT, 0, 18);
    reactive::bind_theme(info_body, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    lv_subject_add_observer_obj(view_model().info_visible_subject(), info_visible_observer, info_panel, nullptr);
}

} // namespace screen
