/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_model.h"

namespace model {
namespace {

// The min/max window shifts together by this many seconds per key press,
// keeping a constant randomization spread between the two bounds.
constexpr int kIntervalStepSeconds = 1;
constexpr int kIntervalFloorSeconds = 1;
constexpr int kIntervalCeilingSeconds = 300;

} // namespace

const char* jiggler_mode_label(JigglerMode mode) {
    switch (mode) {
        case JigglerMode::Mouse:
            return "Mouse";
        case JigglerMode::Keyboard:
            return "Keyboard";
        case JigglerMode::Both:
        default:
            return "Mouse + Keyboard";
    }
}

JigglerMode next_jiggler_mode(JigglerMode mode) {
    switch (mode) {
        case JigglerMode::Mouse:
            return JigglerMode::Keyboard;
        case JigglerMode::Keyboard:
            return JigglerMode::Both;
        case JigglerMode::Both:
        default:
            return JigglerMode::Mouse;
    }
}

const char* BaseModel::app_title() const {
    return "Jiggler";
}

bool BaseModel::dark_mode() const {
    return dark_mode_;
}

void BaseModel::set_dark_mode(bool enabled) {
    dark_mode_ = enabled;
}

void BaseModel::toggle_dark_mode() {
    dark_mode_ = !dark_mode_;
}

bool BaseModel::is_jiggling() const {
    return jiggling_active_;
}

void BaseModel::set_jiggling(bool active) {
    jiggling_active_ = active;
}

JigglerMode BaseModel::jiggler_mode() const {
    return jiggler_mode_;
}

void BaseModel::cycle_jiggler_mode() {
    jiggler_mode_ = next_jiggler_mode(jiggler_mode_);
}

int BaseModel::interval_min_seconds() const {
    return interval_min_seconds_;
}

int BaseModel::interval_max_seconds() const {
    return interval_max_seconds_;
}

void BaseModel::increase_interval() {
    if (interval_max_seconds_ + kIntervalStepSeconds > kIntervalCeilingSeconds) {
        return;
    }
    interval_min_seconds_ += kIntervalStepSeconds;
    interval_max_seconds_ += kIntervalStepSeconds;
}

void BaseModel::decrease_interval() {
    if (interval_min_seconds_ - kIntervalStepSeconds < kIntervalFloorSeconds) {
        return;
    }
    interval_min_seconds_ -= kIntervalStepSeconds;
    interval_max_seconds_ -= kIntervalStepSeconds;
}

int BaseModel::moves_sent() const {
    return moves_sent_;
}

void BaseModel::increment_moves_sent() {
    ++moves_sent_;
}

void BaseModel::reset_moves_sent() {
    moves_sent_ = 0;
}

bool BaseModel::info_visible() const {
    return info_visible_;
}

void BaseModel::toggle_info() {
    info_visible_ = !info_visible_;
}

} // namespace model
