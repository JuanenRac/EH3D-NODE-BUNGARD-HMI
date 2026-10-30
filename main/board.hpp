// EH3D-NODE-BUNGARD-HMI - main/board.hpp: what the application needs from the module (display, touch, stop input, sound).
// GPL-3.0-or-later.
#pragma once

#include <cstdint>

namespace board {

/// Starts the display and the touch and the LVGL port; false when the panel does not answer.
bool display_start();
/// Takes and gives back the lock of LVGL (every call into LVGL from another task is between the two).
void lvgl_lock();
void lvgl_unlock();
/// Brightness of the backlight, 10 to 100.
void set_brightness(unsigned percent);
/// Starts the codec, the speaker and the microphone; false when the codec does not answer.
bool audio_start();
/// Reads the hardware stop input: `wired` is false when no pin is set.
void read_hardware_stop(bool& wired, bool& pressed);
/// Milliseconds since start (it wraps around after 49 days; the gate compares differences).
std::uint32_t now_ms();

}  // namespace board
