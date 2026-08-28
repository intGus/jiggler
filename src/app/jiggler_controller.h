/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_viewmodel.h"
#include "jiggle_output.h"

#include "lvgl.h"

#include <cstdint>
#include <memory>

namespace app {

// Bridges UI intent (view_model.active_subject()) to hardware-facing output
// (platform::JiggleOutput). Owned by Application::run(), same as
// ScreenManager and the shortcut controller: the view model only stores
// state, this class performs the actual timed side effect.
class JigglerController {
public:
    explicit JigglerController(viewmodel::BaseViewModel& view_model);
    ~JigglerController();

    JigglerController(const JigglerController&) = delete;
    JigglerController& operator=(const JigglerController&) = delete;

private:
    void start();
    void stop();
    void schedule_next();
    void fire();
    void tick();

    static void active_observer_cb(lv_observer_t* observer, lv_subject_t* subject);
    static void tick_timer_cb(lv_timer_t* timer);

    viewmodel::BaseViewModel& view_model_;
    std::unique_ptr<platform::JiggleOutput> output_;
    lv_observer_t* active_observer_{nullptr};
    lv_timer_t* tick_timer_{nullptr};
    uint32_t next_fire_at_ms_{0};
    int last_shown_countdown_seconds_{-1};
    // lv_subject_add_observer() invokes the callback immediately with the
    // current value, before this constructor finishes. That first call must
    // not overwrite BaseViewModel's nicer first-launch status text.
    bool initial_sync_{true};
};

} // namespace app
