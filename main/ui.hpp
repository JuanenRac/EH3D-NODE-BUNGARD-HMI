// EH3D-NODE-BUNGARD-HMI - main/ui.hpp: the screens of the touch display. GPL-3.0-or-later.
#pragma once

#include "../core/gate.hpp"
#include "../core/hmi_config.hpp"

namespace ui {

/// What the screens need to know about the machine, refreshed by the application.
struct State {
    bool armed = false;
    bool stop_pressed = false;
    bool stop_latched = true;
    bool stop_wired = false;
};

/// What the operator did on the screen; the application runs it through the gate.
struct Request {
    hmi::Action action;
    bool release_acknowledged = false;
    bool confirmed = false;
    bool cancelled = false;
};

/// Builds the screens (with the LVGL lock taken). `post` is called, in the LVGL task, for each request of the operator.
void build(const hmi::Settings& settings, void (*post)(const Request&));
/// Shows the state (with the LVGL lock taken).
void show(const State& state, const hmi::Settings& settings);
/// Shows the question of the gate, or hides it when `action` is null.
void ask(const hmi::Action* action, const hmi::Settings& settings);
/// Shows why the gate refused.
void refused(hmi::Why why, const hmi::Settings& settings);

}  // namespace ui
