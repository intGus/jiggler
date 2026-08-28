/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <memory>
#include <string>

namespace platform {

// Which HID device role a single jiggle pulse should imitate.
enum class JiggleMode {
    Mouse,
    Keyboard,
    Both,
};

struct JiggleResult {
    bool success{false};
    std::string backend; // e.g. "Simulated (desktop emulator)" or "USB HID gadget"
    std::string detail;  // human-readable outcome or error, safe to show in the UI
};

// Hardware-facing output for a single jiggle action. Desktop builds use a
// simulated backend that never touches real input devices. Device builds
// target the Linux USB HID gadget character devices (/dev/hidg*) and report
// an explicit failure rather than a fake success when the device is not in
// USB Slave/Device mode with a HID gadget configured - see README.md.
class JiggleOutput {
public:
    JiggleOutput() = default;
    virtual ~JiggleOutput() = default;

    JiggleOutput(const JiggleOutput&) = delete;
    JiggleOutput& operator=(const JiggleOutput&) = delete;

    virtual JiggleResult perform(JiggleMode mode) = 0;
};

std::unique_ptr<JiggleOutput> create_jiggle_output();

} // namespace platform
