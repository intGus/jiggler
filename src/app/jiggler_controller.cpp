/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "jiggler_controller.h"

#include <algorithm>
#include <cstdio>
#include <random>

namespace app {
namespace {

constexpr uint32_t kTickPeriodMs = 250;

platform::JiggleMode to_platform_mode(model::JigglerMode mode) {
    switch (mode) {
        case model::JigglerMode::Mouse:
            return platform::JiggleMode::Mouse;
        case model::JigglerMode::Keyboard:
            return platform::JiggleMode::Keyboard;
        case model::JigglerMode::Both:
        default:
            return platform::JiggleMode::Both;
    }
}

std::mt19937& shared_rng() {
    static std::mt19937 rng(std::random_device{}());
    return rng;
}

} // namespace

JigglerController::JigglerController(viewmodel::BaseViewModel& view_model)
    : view_model_(view_model), output_(platform::create_jiggle_output()) {
    active_observer_ =
        lv_subject_add_observer(view_model_.active_subject(), active_observer_cb, this);
    tick_timer_ = lv_timer_create(tick_timer_cb, kTickPeriodMs, this);
    if (tick_timer_) {
        lv_timer_pause(tick_timer_);
    }
}

JigglerController::~JigglerController() {
    if (tick_timer_) {
        lv_timer_delete(tick_timer_);
        tick_timer_ = nullptr;
    }
    if (active_observer_) {
        lv_observer_remove(active_observer_);
        active_observer_ = nullptr;
    }
}

void JigglerController::start() {
    view_model_.reset_moves_sent();
    last_shown_countdown_seconds_ = -1;
    if (tick_timer_) {
        lv_timer_resume(tick_timer_);
        lv_timer_reset(tick_timer_);
    }
    schedule_next();
}

void JigglerController::stop() {
    if (tick_timer_) {
        lv_timer_pause(tick_timer_);
    }
    view_model_.set_status("Stopped - press RUN (key 4) to resume");
}

void JigglerController::schedule_next() {
    int min_seconds = view_model_.interval_min_seconds();
    int max_seconds = view_model_.interval_max_seconds();
    if (max_seconds < min_seconds) {
        std::swap(min_seconds, max_seconds);
    }

    std::uniform_int_distribution<int> distribution(min_seconds, max_seconds);
    const int wait_seconds = distribution(shared_rng());
    next_fire_at_ms_ = lv_tick_get() + static_cast<uint32_t>(wait_seconds) * 1000U;
    last_shown_countdown_seconds_ = -1;
}

void JigglerController::fire() {
    const auto result = output_->perform(to_platform_mode(view_model_.jiggler_mode()));
    view_model_.increment_moves_sent();

    char buffer[64];
    if (result.success) {
        std::snprintf(buffer, sizeof(buffer), "Moved via %s", result.backend.c_str());
    } else {
        std::snprintf(buffer, sizeof(buffer), "Failed: %s", result.detail.c_str());
    }
    view_model_.set_status(buffer);

    schedule_next();
}

void JigglerController::tick() {
    if (!view_model_.is_jiggling()) {
        return;
    }

    const auto now = lv_tick_get();
    const auto remaining_ms = static_cast<int32_t>(next_fire_at_ms_ - now);
    if (remaining_ms <= 0) {
        fire();
        return;
    }

    const int remaining_seconds = (remaining_ms + 999) / 1000;
    if (remaining_seconds == last_shown_countdown_seconds_) {
        return;
    }
    last_shown_countdown_seconds_ = remaining_seconds;

    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "Jiggling - next move in %ds", remaining_seconds);
    view_model_.set_status(buffer);
}

void JigglerController::active_observer_cb(lv_observer_t* observer, lv_subject_t* subject) {
    auto* controller = static_cast<JigglerController*>(lv_observer_get_user_data(observer));
    if (!controller) {
        return;
    }

    if (controller->initial_sync_) {
        // Fires during lv_subject_add_observer() itself, before the
        // constructor returns. BaseModel always starts idle and the tick
        // timer is already created paused, so there is nothing to do beyond
        // leaving BaseViewModel's first-launch status text as-is.
        controller->initial_sync_ = false;
        return;
    }

    if (lv_subject_get_int(subject)) {
        controller->start();
    } else {
        controller->stop();
    }
}

void JigglerController::tick_timer_cb(lv_timer_t* timer) {
    auto* controller = static_cast<JigglerController*>(lv_timer_get_user_data(timer));
    if (controller) {
        controller->tick();
    }
}

} // namespace app
