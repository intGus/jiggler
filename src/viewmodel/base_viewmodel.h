/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "subjects.h"
#include "base_model.h"

#include "lvgl.h"

namespace viewmodel {

class BaseViewModel {
public:
    BaseViewModel();
    ~BaseViewModel();

    BaseViewModel(const BaseViewModel&) = delete;
    BaseViewModel& operator=(const BaseViewModel&) = delete;

    lv_subject_t* title_subject();
    lv_subject_t* dark_mode_subject();
    lv_subject_t* active_subject();
    lv_subject_t* mode_subject();
    lv_subject_t* interval_min_subject();
    lv_subject_t* interval_max_subject();
    lv_subject_t* moves_sent_subject();
    lv_subject_t* status_subject();
    lv_subject_t* info_visible_subject();
    lv_subject_t* quit_requested_subject();

    bool is_dark_mode() const;
    void set_dark_mode(bool enabled);
    void toggle_dark_mode();

    bool is_jiggling() const;
    model::JigglerMode jiggler_mode() const;
    int interval_min_seconds() const;
    int interval_max_seconds() const;

    // UI-triggered actions. These only flip stored state/subjects; the actual
    // HID output side effect is performed by app::JigglerController, which
    // observes active_subject()/status_subject() from src/app.
    void toggle_jiggling();
    void cycle_jiggler_mode();
    void increase_interval();
    void decrease_interval();
    void toggle_info();
    void request_quit();

    // Called by app::JigglerController to report progress back to the UI.
    void set_status(const char* text);
    void increment_moves_sent();
    void reset_moves_sent();

private:
    void publish_all();

    model::BaseModel model_;
    reactive::StringSubject<32> title_subject_;
    reactive::BoolSubject dark_mode_subject_;
    reactive::BoolSubject active_subject_;
    reactive::IntSubject mode_subject_;
    reactive::IntSubject interval_min_subject_;
    reactive::IntSubject interval_max_subject_;
    reactive::IntSubject moves_sent_subject_;
    reactive::StringSubject<64> status_subject_;
    reactive::BoolSubject info_visible_subject_;
    reactive::BoolSubject quit_requested_subject_;
};

} // namespace viewmodel
