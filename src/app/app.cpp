/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "app.h"

#include "application_config.h"
#include "asset_manager.h"
#include "help_popup.h"
#include "jiggler_controller.h"
#include "logger.h"
#include "linux_input.h"
#include "screenshot_service.h"
#include "screen_manager.h"
#include "base_viewmodel.h"
#include "theme.h"
#include "toast.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

#if USE_DESKTOP
#include "desktop_simulator_frame.h"
#endif

#if !USE_DESKTOP
#if APP_USE_DRM
#include "src/drivers/display/drm/lv_linux_drm.h"
#else
#include "src/drivers/display/fb/lv_linux_fbdev.h"
#endif
#endif

#ifndef APP_FRAMEBUFFER_DEVICE
#define APP_FRAMEBUFFER_DEVICE "/dev/fb0"
#endif

#ifndef APP_DRM_DEVICE
#define APP_DRM_DEVICE "/dev/dri/card0"
#endif

#ifndef APP_DRM_CONNECTOR_ID
#define APP_DRM_CONNECTOR_ID -1
#endif

#ifndef APP_CONFIG_FILE
#define APP_CONFIG_FILE "jiggler-app.conf"
#endif

namespace app {
namespace {

void quit_requested_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* running = static_cast<bool*>(lv_observer_get_user_data(observer));
    if (running && lv_subject_get_int(subject)) {
        *running = false;
    }
}

// Saves dark mode plus the last-used jiggler mode and interval whenever one of
// them changes, so the app reopens with the same settings.
struct SettingsPersistence {
    std::string config_path;
    viewmodel::BaseViewModel* view_model;
    ApplicationConfig last;
    bool ready = false; // skip the initial observer callbacks fired on registration
};

std::string writable_config_path() {
#if USE_DESKTOP
    return APP_CONFIG_FILE;
#else
    if (const char* xdg_config_home = std::getenv("XDG_CONFIG_HOME")) {
        const std::filesystem::path root(xdg_config_home);
        if (!root.empty() && root.is_absolute()) {
            return (root / "jiggler-app" / "jiggler-app.conf").string();
        }
    }
    if (const char* home = std::getenv("HOME")) {
        const std::filesystem::path root(home);
        if (!root.empty() && root.is_absolute()) {
            return (root / ".config" / "jiggler-app" / "jiggler-app.conf").string();
        }
    }
    return APP_CONFIG_FILE;
#endif
}

ApplicationConfig current_settings(const viewmodel::BaseViewModel& view_model) {
    ApplicationConfig config;
    config.dark_mode = view_model.is_dark_mode();
    config.has_mode = true;
    config.mode = static_cast<int>(view_model.jiggler_mode());
    config.has_interval_min = true;
    config.interval_min_seconds = view_model.interval_min_seconds();
    return config;
}

void persist_settings_observer(lv_observer_t* observer, lv_subject_t*) {
    auto* persistence = static_cast<SettingsPersistence*>(lv_observer_get_user_data(observer));
    if (!persistence || !persistence->ready || !persistence->view_model) {
        return;
    }
    const auto config = current_settings(*persistence->view_model);
    if (config.dark_mode == persistence->last.dark_mode && config.mode == persistence->last.mode &&
        config.interval_min_seconds == persistence->last.interval_min_seconds) {
        return;
    }
    persistence->last = config;

    std::string error;
    if (save_application_config(persistence->config_path, config, error)) {
        LOG_INFO("saved config: {} (dark_mode={}, mode={}, interval_min={})",
                 persistence->config_path,
                 config.dark_mode ? "yes" : "no",
                 config.mode,
                 config.interval_min_seconds);
    } else {
        LOG_WARN("failed to save config {}: {}", persistence->config_path, error);
    }
}

// Backlight control for the auto screen-off while jiggling. The backlight node
// is group-writable by `video`, so no root is needed. Desktop builds are no-ops.
constexpr const char* kBacklightPath = "/sys/class/backlight/backlight/brightness";
constexpr uint32_t kScreenOffAfterMs = 30000;

bool read_backlight(int& value) {
#if USE_DESKTOP
    (void)value;
    return false;
#else
    std::ifstream in(kBacklightPath);
    return static_cast<bool>(in >> value);
#endif
}

bool write_backlight(int value) {
#if USE_DESKTOP
    (void)value;
    return false;
#else
    std::ofstream out(kBacklightPath);
    out << value;
    out.flush();
    return static_cast<bool>(out);
#endif
}

class ShortcutController {
public:
    ShortcutController(viewmodel::BaseViewModel& view_model, AssetManager& assets)
        : view_model_(view_model),
          toast_(std::make_unique<view::widgets::Toast>(lv_layer_top(), view_model, assets)),
          help_popup_(std::make_unique<view::widgets::HelpPopup>(lv_layer_top(), view_model, assets)) {
        toast_->build();
        help_popup_->build();
        // Only the right-side port, in Device mode, can act as a USB mouse/keyboard.
        toast_->show_highlighted("Set USB switch to ",
                                 "Device",
                                 " and use the right-side port",
                                 view::widgets::ToastTone::Warning,
                                 6000,
                                 false);
        platform::set_global_key_listener(global_key_listener, this);
        platform::set_key_release_listener(key_release_listener, this);
        last_key_tick_ = lv_tick_get();
        screen_timer_ = lv_timer_create(screen_timer_cb, 1000, this);
    }

    ~ShortcutController() {
        if (screen_timer_) {
            lv_timer_delete(screen_timer_);
        }
        wake_screen(); // never leave the device with a dark screen
        platform::clear_key_release_listener(key_release_listener, this);
        platform::clear_global_key_listener(global_key_listener, this);
    }

private:
    // Screen off after kScreenOffAfterMs without key presses, only while
    // jiggling. The key that wakes the screen is swallowed (press, long-press
    // and release) so it never triggers its action, e.g. a held ESC quitting.
    void check_screen_timeout() {
        if (screen_off_ || !view_model_.is_jiggling()) {
            return;
        }
        if (lv_tick_elaps(last_key_tick_) < kScreenOffAfterMs) {
            return;
        }
        int current = 0;
        if (!read_backlight(current)) {
            return;
        }
        saved_brightness_ = current > 0 ? current : 100;
        if (write_backlight(0)) {
            screen_off_ = true;
            LOG_INFO("screen off (idle while jiggling)");
        }
    }

    void wake_screen() {
        if (!screen_off_) {
            return;
        }
        write_backlight(saved_brightness_);
        screen_off_ = false;
        LOG_INFO("screen on");
    }

    static void screen_timer_cb(lv_timer_t* timer) {
        auto* controller = static_cast<ShortcutController*>(lv_timer_get_user_data(timer));
        if (controller) {
            controller->check_screen_timeout();
        }
    }

    bool handle_key(uint32_t key, bool long_pressed) {
        last_key_tick_ = lv_tick_get();
        if (screen_off_) {
            wake_screen();
            wake_key_ = key;
            return true;
        }
        if (wake_key_ != 0 && key == wake_key_) {
            return true; // long-press of the key that woke the screen
        }

        if (key == platform::kKeyPrintScreen) {
            if (!long_pressed) {
                const auto result = platform::screenshot::capture_active_screen();
                if (result.success) {
                    LOG_INFO("screenshot saved: {}", result.path);
                    toast_->show_highlighted("Saved to ",
                                             result.path,
                                             "",
                                             view::widgets::ToastTone::Success,
                                             5200,
                                             true);
                }
                else {
                    LOG_WARN("screenshot failed: {}", result.error);
                    toast_->show("Screenshot failed: " + result.error,
                                 view::widgets::ToastTone::Error,
                                 3200);
                }
            }
            return true;
        }

        if (key == platform::kKeyHelp) {
            if (!long_pressed) {
                toast_->hide();
                if (help_popup_->visible()) {
                    help_popup_->hide();
                }
                else {
                    help_popup_->show();
                }
            }
            return true;
        }

        if (help_popup_->visible()) {
            if (key == LV_KEY_ESC && !long_pressed) {
                help_popup_->hide();
            }
            return true;
        }

        // ESC/HOME is a dedicated physical key, separate from the F4-F8 row,
        // so it is reserved for exit: tap alone does nothing but flash a
        // reminder, only a ~700ms hold actually quits. Key 4 stays free for
        // its bound nav action (start/stop jiggling) and is handled by the
        // normal dispatch_nav_key() path once this returns false below.
        if (key != LV_KEY_ESC) {
            return false;
        }

        if (long_pressed) {
            if (exit_key_pressed_ == key && exit_armed_) {
                toast_->hide();
                view_model_.request_quit();
            }
        }
        else {
            exit_key_pressed_ = key;
            exit_armed_ = true;
            toast_->show_persistent_highlighted("Hold ",
                                                "ESC",
                                                " to Exit",
                                                view::widgets::ToastTone::Warning);
        }
        return true;
    }

    void handle_release(uint32_t key) {
        if (wake_key_ != 0 && key == wake_key_) {
            wake_key_ = 0;
            return;
        }
        if (key != exit_key_pressed_) {
            return;
        }

        if (exit_armed_) {
            toast_->hide();
        }
        exit_key_pressed_ = 0;
        exit_armed_ = false;
    }

    static bool global_key_listener(uint32_t key,
                                    const char*,
                                    bool long_pressed,
                                    void* user_data) {
        auto* controller = static_cast<ShortcutController*>(user_data);
        return controller && controller->handle_key(key, long_pressed);
    }

    static void key_release_listener(uint32_t key, const char*, void* user_data) {
        auto* controller = static_cast<ShortcutController*>(user_data);
        if (controller) {
            controller->handle_release(key);
        }
    }

    viewmodel::BaseViewModel& view_model_;
    std::unique_ptr<view::widgets::Toast> toast_;
    std::unique_ptr<view::widgets::HelpPopup> help_popup_;
    uint32_t exit_key_pressed_{0};
    lv_timer_t* screen_timer_{nullptr};
    uint32_t last_key_tick_{0};
    uint32_t wake_key_{0};
    int saved_brightness_{100};
    bool screen_off_{false};
    bool exit_armed_{false};
};

#if !USE_DESKTOP
lv_display_t* init_device_display() {
#if APP_USE_DRM
    auto* display = lv_linux_drm_create();
    if (!display) {
        return nullptr;
    }

    if (lv_linux_drm_set_file(display, APP_DRM_DEVICE, APP_DRM_CONNECTOR_ID) != LV_RESULT_OK) {
        lv_display_delete(display);
        return nullptr;
    }

    platform::init_key_input(display);
    return display;
#else
    auto* display = lv_linux_fbdev_create();
    if (!display) {
        return nullptr;
    }

    if (lv_linux_fbdev_set_file(display, APP_FRAMEBUFFER_DEVICE) != LV_RESULT_OK) {
        lv_display_delete(display);
        return nullptr;
    }

    platform::init_key_input(display);
    return display;
#endif
}
#endif

} // namespace

int Application::run() {
    logger::Logger::init();
    logger::Logger::set_tag("jiggler");

    lv_init();

    AssetManager assets;
    for (const auto& root : assets.roots()) {
        LOG_INFO("asset root: {}", root.string());
    }

    viewmodel::BaseViewModel view_model;
    const std::string user_config_path = writable_config_path();
    std::string loaded_config_path = APP_CONFIG_FILE;
    if (user_config_path != APP_CONFIG_FILE) {
        std::error_code filesystem_error;
        if (std::filesystem::is_regular_file(user_config_path, filesystem_error)) {
            loaded_config_path = user_config_path;
        }
    }

    ApplicationConfig config;
    std::string config_error;
    if (load_application_config(loaded_config_path, config, config_error)) {
        view_model.set_dark_mode(config.dark_mode);
        if (config.has_mode && config.mode >= 0 && config.mode <= 2) {
            view_model.set_jiggler_mode(static_cast<model::JigglerMode>(config.mode));
        }
        if (config.has_interval_min) {
            view_model.set_interval_min_seconds(config.interval_min_seconds);
        }
        LOG_INFO("loaded config: {} (dark_mode={}, mode={}, interval_min={})",
                 loaded_config_path,
                 config.dark_mode ? "yes" : "no",
                 static_cast<int>(view_model.jiggler_mode()),
                 view_model.interval_min_seconds());
    } else {
        LOG_WARN("failed to load config {}: {}; using defaults", loaded_config_path, config_error);
    }

#if USE_DESKTOP
    DesktopSimulatorFrame simulator_frame(assets);
    auto* display = simulator_frame.display();
#else
    auto* display = init_device_display();
#endif
    if (!display) {
        LOG_ERROR("failed to initialize display");
        return 1;
    }

    auto* standard_font = assets.load_standard_font(14);
    view::apply_lvgl_theme(display,
                           view_model.is_dark_mode(),
                           standard_font ? standard_font : LV_FONT_DEFAULT);

#if USE_DESKTOP
    simulator_frame.bind_dark_mode(view_model.dark_mode_subject());
#endif

    ScreenManager screen_manager(view_model, assets);
    screen_manager.start();
    ShortcutController shortcuts(view_model, assets);
    JigglerController jiggler_controller(view_model);

    SettingsPersistence settings_persistence{user_config_path, &view_model, current_settings(view_model)};
    auto* dark_mode_observer = lv_subject_add_observer(view_model.dark_mode_subject(),
                                                        persist_settings_observer,
                                                        &settings_persistence);
    auto* mode_observer = lv_subject_add_observer(view_model.mode_subject(),
                                                  persist_settings_observer,
                                                  &settings_persistence);
    auto* interval_observer = lv_subject_add_observer(view_model.interval_min_subject(),
                                                      persist_settings_observer,
                                                      &settings_persistence);
    settings_persistence.ready = true;

    bool running = true;
    auto* quit_observer = lv_subject_add_observer(view_model.quit_requested_subject(),
                                                  quit_requested_observer,
                                                  &running);

    LOG_INFO("LVGL app started at {}x{}", lv_display_get_horizontal_resolution(display),
             lv_display_get_vertical_resolution(display));
    while (running
#if USE_DESKTOP
           && simulator_frame.process_events()
#endif
    ) {
        // lv_delay_ms() busy-waits unless a delay callback is registered, which
        // pinned a CPU core at 100% on the device. Sleep for real instead, for as
        // long as LVGL says nothing is due (capped so key input stays responsive).
        uint32_t idle_ms = lv_timer_handler();
        if (idle_ms == LV_NO_TIMER_READY || idle_ms > 30) {
            idle_ms = 30;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(idle_ms ? idle_ms : 1));
    }

    if (quit_observer) {
        lv_observer_remove(quit_observer);
    }
    if (dark_mode_observer) {
        lv_observer_remove(dark_mode_observer);
    }
    if (mode_observer) {
        lv_observer_remove(mode_observer);
    }
    if (interval_observer) {
        lv_observer_remove(interval_observer);
    }

    return 0;
}

} // namespace app
