// EH3D-NODE-BUNGARD-HMI - main/ui.hpp: the screens of the touch display. GPL-3.0-or-later.
#pragma once

#include <string>
#include <vector>

#include "../core/gate.hpp"
#include "../core/hmi_config.hpp"
#include "../core/job_card.hpp"
#include "../core/job_tools.hpp"
#include "../core/orientation.hpp"
#include "../core/rack.hpp"
#include "../core/quality.hpp"
#include "../core/sdfiles.hpp"
#include "../core/server_link.hpp"
#include "../core/status.hpp"
#include "../core/workflow.hpp"

namespace ui {

/// What the screens need to know about the machine, refreshed by the application.
struct State {
    bool armed = false;
    bool stop_pressed = false;
    bool stop_latched = true;
    bool stop_wired = false;
    bool server_ok = false;
    hmi::Link link = hmi::Link::Unknown;  // connected, no connection or another server
    bool stale = false;                   // the data shown are older than `stale_after_s`
    std::uint32_t age_s = 0;              // how old they are
    int percent = 0;
    std::string note;  // the line under the buttons: why something was refused, what the server answered
};

enum class What { Action, AckRelease, Confirm, Cancel, SetOrientation, SetLanguage, OpenCard, SendJob, RefreshCard, LightSet, InspectRun, InspectPreset, InspectCalibrate, ToggleTool, Resume, SetSpindleOver, JogPress, JogRelease, JogStep, JogMode, JogSpeed, JogPlace, JogHome, RackSet };

/// What the operator did on the screen; the application runs it through the gate.
struct Request {
    What what = What::Action;
    hmi::Action action;
    hmi::Orientation orientation = hmi::Orientation::Auto;
    hmi::Lang language = hmi::Lang::En;  // SetLanguage: the language the operator chose
    char file[64] = {0};  // the name of the file of the card for OpenCard
    int light = 0;        // LightSet and InspectRun: the index in hmi::kLightLevels
    int hole = 0;         // InspectRun: the index in hmi::kHoleWidths
    int preset = 0;       // InspectPreset: 0 standard, 1 fine pitch, 2 prototype
    int tool = 0;         // ToggleTool: the number of the tool (the pen of a routing file) to switch off or on
    bool refuse = false;  // SetSpindleOver: true = refuse an order beyond the spindle, false = cut it to the top speed
    int index = 0;        // JogPress: the arrow (0 X-, 1 X+, 2 Y-, 3 Y+, 4 Z down, 5 Z up); JogStep: the step; JogSpeed: 0 slow, 1 medium, 2 fast; JogPlace: the place 0..3
    bool save = false;    // JogPlace: keep the place of the head there instead of going to it
    bool route_tool = false;  // RackSet: the tool is a cutter (a routing pen), not a drill
    int slot = 0;         // RackSet: the place of the bar (0 takes it away)
};

/// Builds the screens for a layout (with the LVGL lock taken). `post` is called, in the LVGL task, for each request of the operator.
void build(const hmi::Settings& settings, const hmi::Layout& layout, void (*post)(const Request&));
/// Shows the state (with the LVGL lock taken).
void show(const State& state, const hmi::Settings& settings);
/// Shows the question of the gate with the order in the language of the operator and the seconds left before it aborts by itself, or hides it when `action` is null. `extra` is
/// the summary of the plan under the question (the estimated time and what looks wrong in the work) when the order is the start of a job.
void ask(const hmi::Action* action, const hmi::Settings& settings, std::uint32_t remaining_ms = 0, const std::string& extra = "");
/// Updates the countdown of the question that is showing.
void ask_tick(std::uint32_t remaining_ms, const hmi::Settings& settings);
/// The version of the firmware, shown in the title bar and in the settings (set it before `build`).
void set_version(const std::string& version);
/// The line of the settings file that could not be used, or that hmi.cfg is missing (empty: all is well); it stays on the screen under the top bar.
void show_config_issue(const std::string& text);
/// Shows why the gate refused.
void refused(hmi::Why why, const hmi::Settings& settings);
/// Shows the machines of the server, each with its state and its job; when the data are old each row says so.
void show_fleet(const hmi::FleetView& fleet, const hmi::Settings& settings, bool stale = false);
/// Shows the quality check: the verdict of the last one, the ring of LEDs, and whether a check is running now.
void show_quality(const hmi::QualityView& quality, const hmi::LightView& light, bool running, const hmi::Settings& settings);
/// Shows what the chosen file holds: a chip for each tool (a tap switches it off or on), how many holes or cuts stay in the job, and the spindle of the machine.
/// Under the tools it says when the board does not fit the table (it does not block the job) and what the server noted about the job it made.
void show_job(const hmi::ToolSwitch& tools, const hmi::SpindleView& spindle, const hmi::JobAdvice& advice, const hmi::Settings& settings);
/// Where the job is, for the job page: a job stopped part way (and whether it can go on from there), the holes (cuts) already done, and the program that comes next.
struct FlowState {
    bool stopped = false;
    bool failed = false;     // the stopped job failed (the server's reason is in `message`) instead of being cancelled
    bool can_resume = false;
    bool held = false;       // the job is held by the operator (pause): the button that lets it go on
    bool tool_wait = false;  // the job waits at a tool change: the button that answers it
    bool route = false;     // the program in turn is a routing one
    int done = 0;           // stopped: the holes (paths) the stopped job did
    int total = 0;          // stopped: the holes (paths) it had
    int skipped = 0;        // the holes (paths) already done before the job in turn
    std::string next;       // the name of the file of the program that comes next, empty when none
    std::string message;    // why the job stopped, as the server says it
    std::string resumed_from;  // the id of the stopped job that the job in turn goes on from, empty when none
};
/// Shows where the job is: what a stopped job did and the button to go on from there, and the program that comes next.
void show_flow(const FlowState& flow, const hmi::Settings& settings);
/// Shows the lines of the machine page about the work in hand (the model, the tool at work, the next one, the tools finished and the time left); empty text clears them.
void show_hud(const std::string& text);
/// Shows the files of the card.
void show_card(const std::vector<hmi::Entry>& files, const hmi::Settings& settings);

/// The panel that moves the machine by hand, as the application keeps it.
struct JogView {
    std::vector<double> steps;
    int step = 0;
    bool continuous = false;
    int speed = 1;            // 0 slow, 1 medium, 2 fast
    double feeds[3] = {0, 0, 0};
    bool has_place[4] = {false, false, false, false};
    bool pos_known = false;   // the head is where the panel has put it (the machine does not tell it)
    double x = 0, y = 0, z = 0;
    std::string note;         // why the last press did not move the machine
};
/// Shows the panel that moves the machine by hand.
void show_jog(const JogView& view, const hmi::Settings& settings);
/// Shows the four blocks of the card of the work in hand (the tool and its step, the times, the position and the depth, the counters).
void show_job_card(const hmi::CardText& card);
/// The bar of the tool changer: the squares of the tools with their places and states, and the window to give each tool its place.
struct RackView {
    std::vector<hmi::BarTool> tools;
    std::vector<int> places;      // the places of the bar the machine defines
    std::string problems;         // what stops the start (empty when all is well)
    std::string not_chained;      // the red line when the server did not chain the tool changes (empty otherwise)
    bool atc = false;             // the machine changes the tools itself: the bar and its window show only then
};
void show_rack(const RackView& view, const hmi::Settings& settings);
/// The window between the stages: the drilling is done and the routing comes next, with the buttons to follow the routing or to stop; an empty text hides it.
void show_stage(const std::string& text, const hmi::Settings& settings);

}  // namespace ui
