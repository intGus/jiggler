#include "jiggle_output.h"

#include "lvgl.h"

#include <cstdio>
#include <random>

#if !USE_DESKTOP
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace platform {
namespace {

std::mt19937& shared_rng() {
    static std::mt19937 rng(std::random_device{}());
    return rng;
}

// A noticeable but short pointer offset (12-24 px per axis, random sign).
// On real hosts with pointer acceleration a +/-4 move was barely one pixel.
int nonzero_delta() {
    std::uniform_int_distribution<int> magnitude(12, 24);
    std::bernoulli_distribution negative(0.5);
    const int value = magnitude(shared_rng());
    return negative(shared_rng()) ? -value : value;
}

#if USE_DESKTOP

// Desktop/emulator backend. There is no real pointer or keyboard to drive
// here, so this only produces a plausible outcome for the UI to display; it
// never claims to have touched host input devices.
class SimulatedJiggleOutput : public JiggleOutput {
public:
    JiggleResult perform(JiggleMode mode) override {
        JiggleResult result;
        result.success = true;
        result.backend = "Simulated (desktop emulator)";

        const int dx = nonzero_delta();
        const int dy = nonzero_delta();
        char buffer[64];
        switch (mode) {
            case JiggleMode::Mouse:
                std::snprintf(buffer, sizeof(buffer), "mouse dx=%d dy=%d", dx, dy);
                break;
            case JiggleMode::Keyboard:
                std::snprintf(buffer, sizeof(buffer), "Shift pulse");
                break;
            case JiggleMode::Both:
            default:
                std::snprintf(buffer, sizeof(buffer), "mouse dx=%d dy=%d + Shift", dx, dy);
                break;
        }
        result.detail = buffer;
        LV_LOG_INFO("jiggle (simulated): %s", result.detail.c_str());
        return result;
    }
};

#else

// Device backend. Writes standard USB HID boot-protocol reports to the
// gadget character devices. This assumes the OS has already put the port in
// USB Slave/Device mode and bound a composite HID gadget exposing
// /dev/hidg0 (keyboard) and /dev/hidg1 (mouse) - this app does not create or
// own that ConfigFS gadget itself, since that is a privileged, system-level,
// one-time setup step, not a per-app action. When those nodes are missing or
// not writable, perform() reports that explicitly rather than pretending the
// jiggle happened. This path has not been exercised against real
// CardputerZero hardware yet (device not released as of writing) and should
// be re-verified once a unit is available.
constexpr const char* kKeyboardHidPath = "/dev/hidg0";
constexpr const char* kMouseHidPath    = "/dev/hidg1";

bool write_report(const char* path, const uint8_t* report, size_t length, std::string& error) {
    const int fd = open(path, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        error = std::string(path) + ": open failed: " + std::strerror(errno);
        return false;
    }

    const ssize_t written = ::write(fd, report, length);
    const int write_errno = errno;
    ::close(fd);

    if (written != static_cast<ssize_t>(length)) {
        if (written < 0 && write_errno == EAGAIN) {
            error = std::string(path) + ": no USB host is reading yet (EAGAIN)";
        } else {
            error = std::string(path) + ": write failed: " + std::strerror(write_errno);
        }
        return false;
    }
    return true;
}

bool send_mouse_jiggle(std::string& error) {
    if (access(kMouseHidPath, W_OK) != 0) {
        error = std::string(kMouseHidPath) +
                " not available - plug in via a USB port set to Slave/Device mode "
                "with the HID mouse gadget configured";
        return false;
    }

    // Standard 4-byte boot-protocol relative mouse report: [buttons, dx, dy, wheel].
    const auto dx = static_cast<uint8_t>(static_cast<int8_t>(nonzero_delta()));
    const auto dy = static_cast<uint8_t>(static_cast<int8_t>(nonzero_delta()));
    const uint8_t report[4] = {0, dx, dy, 0};
    if (!write_report(kMouseHidPath, report, sizeof(report), error)) {
        return false;
    }
    // Move back after a short pause so the pointer does not drift away
    // over many jiggles.
    ::usleep(120 * 1000);
    const uint8_t back[4] = {0,
                             static_cast<uint8_t>(-static_cast<int8_t>(dx)),
                             static_cast<uint8_t>(-static_cast<int8_t>(dy)),
                             0};
    std::string back_error;
    write_report(kMouseHidPath, back, sizeof(back), back_error); // best-effort
    return true;
}

bool send_keyboard_jiggle(std::string& error) {
    if (access(kKeyboardHidPath, W_OK) != 0) {
        error = std::string(kKeyboardHidPath) +
                " not available - plug in via a USB port set to Slave/Device mode "
                "with the HID keyboard gadget configured";
        return false;
    }

    // Standard 8-byte boot-protocol keyboard report: [modifier, reserved, key0..5].
    // A modifier-only pulse (Left Shift, no keycode) registers activity
    // without typing a visible character into whatever has focus on the host.
    constexpr uint8_t kLeftShiftBit = 0x02;
    const uint8_t press[8]   = {kLeftShiftBit, 0, 0, 0, 0, 0, 0, 0};
    const uint8_t release[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    if (!write_report(kKeyboardHidPath, press, sizeof(press), error)) {
        return false;
    }
    std::string release_error;
    write_report(kKeyboardHidPath, release, sizeof(release), release_error); // best-effort
    return true;
}

class UsbHidGadgetOutput : public JiggleOutput {
public:
    JiggleResult perform(JiggleMode mode) override {
        JiggleResult result;
        result.backend = "USB HID gadget";

        std::string error;
        bool success = false;
        switch (mode) {
            case JiggleMode::Mouse:
                success = send_mouse_jiggle(error);
                break;
            case JiggleMode::Keyboard:
                success = send_keyboard_jiggle(error);
                break;
            case JiggleMode::Both:
            default: {
                std::string mouse_error;
                std::string keyboard_error;
                const bool mouse_ok    = send_mouse_jiggle(mouse_error);
                const bool keyboard_ok = send_keyboard_jiggle(keyboard_error);
                success = mouse_ok && keyboard_ok;
                error   = !mouse_error.empty() ? mouse_error : keyboard_error;
                break;
            }
        }

        result.success = success;
        result.detail  = success ? "sent" : error;
        if (!success) {
            LV_LOG_WARN("jiggle failed: %s", error.c_str());
        }
        return result;
    }
};

#endif

} // namespace

std::unique_ptr<JiggleOutput> create_jiggle_output() {
#if USE_DESKTOP
    return std::make_unique<SimulatedJiggleOutput>();
#else
    return std::make_unique<UsbHidGadgetOutput>();
#endif
}

} // namespace platform
