/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_viewmodel.h"

namespace viewmodel {
namespace {

int mode_to_int(model::JigglerMode mode) {
    return static_cast<int>(mode);
}

} // namespace

BaseViewModel::BaseViewModel()
    : title_subject_(model_.app_title()),
      dark_mode_subject_(model_.dark_mode()),
      active_subject_(model_.is_jiggling()),
      mode_subject_(mode_to_int(model_.jiggler_mode())),
      interval_min_subject_(model_.interval_min_seconds()),
      interval_max_subject_(model_.interval_max_seconds()),
      moves_sent_subject_(model_.moves_sent()),
      status_subject_("Idle - press RUN (key 4) to start"),
      info_visible_subject_(model_.info_visible()),
      quit_requested_subject_(false) {}

BaseViewModel::~BaseViewModel() = default;

lv_subject_t* BaseViewModel::title_subject() {
    return title_subject_.native();
}

lv_subject_t* BaseViewModel::dark_mode_subject() {
    return dark_mode_subject_.native();
}

lv_subject_t* BaseViewModel::active_subject() {
    return active_subject_.native();
}

lv_subject_t* BaseViewModel::mode_subject() {
    return mode_subject_.native();
}

lv_subject_t* BaseViewModel::interval_min_subject() {
    return interval_min_subject_.native();
}

lv_subject_t* BaseViewModel::interval_max_subject() {
    return interval_max_subject_.native();
}

lv_subject_t* BaseViewModel::moves_sent_subject() {
    return moves_sent_subject_.native();
}

lv_subject_t* BaseViewModel::status_subject() {
    return status_subject_.native();
}

lv_subject_t* BaseViewModel::info_visible_subject() {
    return info_visible_subject_.native();
}

lv_subject_t* BaseViewModel::quit_requested_subject() {
    return quit_requested_subject_.native();
}

bool BaseViewModel::is_dark_mode() const {
    return model_.dark_mode();
}

void BaseViewModel::set_dark_mode(bool enabled) {
    model_.set_dark_mode(enabled);
    publish_all();
}

void BaseViewModel::toggle_dark_mode() {
    model_.toggle_dark_mode();
    publish_all();
}

bool BaseViewModel::is_jiggling() const {
    return model_.is_jiggling();
}

model::JigglerMode BaseViewModel::jiggler_mode() const {
    return model_.jiggler_mode();
}

int BaseViewModel::interval_min_seconds() const {
    return model_.interval_min_seconds();
}

int BaseViewModel::interval_max_seconds() const {
    return model_.interval_max_seconds();
}

void BaseViewModel::toggle_jiggling() {
    model_.set_jiggling(!model_.is_jiggling());
    active_subject_.set(model_.is_jiggling());
}

void BaseViewModel::cycle_jiggler_mode() {
    model_.cycle_jiggler_mode();
    mode_subject_.set(mode_to_int(model_.jiggler_mode()));
}

void BaseViewModel::set_jiggler_mode(model::JigglerMode mode) {
    model_.set_jiggler_mode(mode);
    mode_subject_.set(mode_to_int(model_.jiggler_mode()));
}

void BaseViewModel::set_interval_min_seconds(int seconds) {
    model_.set_interval_min_seconds(seconds);
    interval_min_subject_.set(model_.interval_min_seconds());
    interval_max_subject_.set(model_.interval_max_seconds());
}

void BaseViewModel::increase_interval() {
    model_.increase_interval();
    interval_min_subject_.set(model_.interval_min_seconds());
    interval_max_subject_.set(model_.interval_max_seconds());
}

void BaseViewModel::decrease_interval() {
    model_.decrease_interval();
    interval_min_subject_.set(model_.interval_min_seconds());
    interval_max_subject_.set(model_.interval_max_seconds());
}

void BaseViewModel::toggle_info() {
    model_.toggle_info();
    info_visible_subject_.set(model_.info_visible());
}

void BaseViewModel::request_quit() {
    quit_requested_subject_.set(true);
}

void BaseViewModel::set_status(const char* text) {
    status_subject_.set(text);
}

void BaseViewModel::increment_moves_sent() {
    model_.increment_moves_sent();
    moves_sent_subject_.set(model_.moves_sent());
}

void BaseViewModel::reset_moves_sent() {
    model_.reset_moves_sent();
    moves_sent_subject_.set(model_.moves_sent());
}

void BaseViewModel::publish_all() {
    title_subject_.set(model_.app_title());
    dark_mode_subject_.set(model_.dark_mode());
}

} // namespace viewmodel
