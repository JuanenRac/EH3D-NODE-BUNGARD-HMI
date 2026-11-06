// EH3D-NODE-BUNGARD-HMI - core/jog_panel.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// The panel that moves the machine by hand, without a screen: a cross for X and Y and a column for Z, steps of 0.01, 0.1, 1, 10 and 50 mm (the list is in hmi.cfg), one step per
// press or a held arrow that goes on in stretches, three speeds, four places kept and "go to". Each press gives the moves it should and the reasons it should not move:
// a job runs, the emergency stop is on, the head is at the end of the travel, or it is too low to cross the board (Z goes up first). The window asks this and keeps only the
// buttons. Z is positive downwards from home. Nothing here sends anything to the machine: the orders are returned to the application, which runs them through the gate.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "hmi_config.hpp"

namespace hmi {

struct JogPos {
    double x = 0, y = 0, z = 0;
    bool operator==(const JogPos& o) const { return x == o.x && y == o.y && z == o.z; }
};

/// The six arrows of the panel.
enum class JogDir { XMinus, XPlus, YMinus, YPlus, ZUp, ZDown };

/// Why a press did not move the machine.
enum class JogRefusal { None, Job, Estop, Limit, RaiseZ };

/// What forbids a move at this moment, told by the application.
struct JogGate {
    bool job_running = false;
    bool estop = false;
};

/// A move relative to where the head is, in mm (Z positive downwards), with the feed it is asked at, in mm per minute.
struct JogOrder {
    double dx = 0, dy = 0, dz = 0, feed = 0;
};

/// The travel the head may use: the table along X and Y, and Z from home down to the surface of the board (and a little below when the person allows it).
struct JogLimits {
    bool enforce = true;      // false (the switch `jog_limits`): the head is not held inside the table
    bool keep_clear = true;   // false (the switch `jog_safe_z`): Z does not have to go up before the head crosses the board
    double x_max = 0, y_max = 0;
    double z_floor = 0;       // the deepest Z, mm below home
    double safe_z = 0;        // across the board the head must be at this depth or higher
};

inline double jog_clamp(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

/// The limits from the table of the machine and the settings.
inline JogLimits make_jog_limits(double table_w, double table_d, const Settings& s) {
    JogLimits l;
    l.enforce = s.jog_limits;
    l.keep_clear = s.jog_safe_z;
    l.x_max = table_w > 0 ? table_w : 0;
    l.y_max = table_d > 0 ? table_d : 0;
    l.z_floor = std::max(0.0, s.jog_board_z_mm + std::max(0.0, s.jog_below_board_mm));
    l.safe_z = jog_clamp(s.jog_board_z_mm - std::max(0.0, s.jog_clearance_mm), 0.0, l.z_floor);
    return l;
}

class JogPanel {
public:
    static constexpr double kEps = 1e-9;
    static constexpr int kMaxChunksPerTick = 8;
    static constexpr int kPlaces = 4;

    JogPanel(const Settings& s, const JogLimits& limits) : limits_(limits) { apply(s); }

    /// Takes the values of the settings again (a settings change); where the head is and the places kept stay.
    void apply(const Settings& s) {
        steps_ = s.jog_steps.empty() ? std::vector<double>{1} : s.jog_steps;
        max_mm_ = static_cast<double>(s.jog_max_mm);
        chunk_mm_ = s.jog_chunk_mm;
        feeds_[0] = s.jog_speed_slow;
        feeds_[1] = s.jog_speed_medium;
        feeds_[2] = s.jog_speed_fast;
        block_running_ = s.jog_block_running;
        if (!applied_) {
            step_ = nearest(s.jog_step);
            continuous_ = s.jog_continuous;
            speed_ = s.jog_speed;
            applied_ = true;
        } else if (step_ >= steps_.size()) {
            step_ = steps_.size() - 1;
        }
    }

    void set_limits(const JogLimits& l) { limits_ = l; }
    const JogLimits& limits() const { return limits_; }

    // ---- the steps, the mode and the speed
    const std::vector<double>& steps() const { return steps_; }
    std::size_t step_index() const { return step_; }
    /// The distance of one step, cut to the longest single move the settings allow (`jog_max_mm`).
    double step_mm() const { return std::min(steps_[step_], max_mm_); }
    void set_step(std::size_t i) {
        if (i < steps_.size()) step_ = i;
    }
    void step_up() { set_step(step_ + 1); }
    void step_down() { set_step(step_ > 0 ? step_ - 1 : 0); }
    bool continuous() const { return continuous_; }
    void set_continuous(bool on) {
        continuous_ = on;
        if (!on) held_ = false;
    }
    JogSpeed speed() const { return speed_; }
    void set_speed(JogSpeed s) { speed_ = s; }
    double feed() const { return feed_of(speed_); }
    double feed_of(JogSpeed s) const { return feeds_[s == JogSpeed::Slow ? 0 : (s == JogSpeed::Medium ? 1 : 2)]; }

    // ---- where the head is
    const JogPos& pos() const { return pos_; }
    /// The place the machine reports (the server says it), when the head was moved by something else.
    void set_pos(const JogPos& p) { pos_ = p; }
    void park() {
        pos_ = JogPos{};
        held_ = false;
        touched_ = false;
    }
    bool touched() const { return touched_; }
    bool held() const { return held_; }
    JogDir held_dir() const { return dir_; }

    // ---- the presses
    /// An arrow is pressed. In the step mode the head moves one step; in the continuous mode the arrow is held and `tick` sends the stretches. The reason is `None` when the press
    /// moves (or starts to hold).
    JogRefusal press(JogDir dir, const JogGate& gate, std::vector<JogOrder>& out) {
        out.clear();
        const JogRefusal g = check(gate);
        if (g != JogRefusal::None) return g;
        if (continuous_) {
            // an arrow that cannot move the head is not held
            if (across(dir) && limits_.enforce && limits_.keep_clear && pos_.z > limits_.safe_z + kEps) return JogRefusal::RaiseZ;
            held_ = true;
            dir_ = dir;
            owed_s_ = chunk_s();
            return JogRefusal::None;
        }
        JogOrder o;
        const JogRefusal r = go(dir, step_mm(), o);
        if (r == JogRefusal::None) out.push_back(o);
        return r;
    }

    /// The arrow is let go.
    void release() { held_ = false; }

    /// `dt` seconds have passed: the stretches of a held arrow that are due go out, one every `chunk / speed`.
    JogRefusal tick(double dt, const JogGate& gate, std::vector<JogOrder>& out) {
        out.clear();
        if (!held_) return JogRefusal::None;
        const JogRefusal g = check(gate);
        if (g != JogRefusal::None) {
            held_ = false;
            return g;
        }
        owed_s_ += dt > 0 ? dt : 0;
        const double every = chunk_s();
        const double chunk = std::min(chunk_mm_, max_mm_);
        while (owed_s_ + kEps >= every && out.size() < static_cast<std::size_t>(kMaxChunksPerTick)) {
            owed_s_ -= every;
            JogOrder o;
            const JogRefusal r = go(dir_, chunk, o);
            if (r != JogRefusal::None) {
                // the end of the travel ends the hold; what was sent before it still goes
                held_ = false;
                return out.empty() ? r : JogRefusal::None;
            }
            out.push_back(o);
        }
        if (out.size() >= static_cast<std::size_t>(kMaxChunksPerTick)) owed_s_ = 0;
        return JogRefusal::None;
    }

    /// Esc: the arrow is let go. A real machine has been sent its orders and cannot be called back.
    void stop() { held_ = false; }

    /// Goes to `target` (inside the travel): Z goes up to the safe height first if the head is lower and has to move across, then across, then Z to its depth.
    JogRefusal goto_pos(JogPos target, const JogGate& gate, std::vector<JogOrder>& out) {
        out.clear();
        const JogRefusal g = check(gate);
        if (g != JogRefusal::None) return g;
        held_ = false;
        const JogPos to = limits_.enforce ? clamp(target) : target;
        double z = pos_.z;
        const bool across_move = std::fabs(to.x - pos_.x) > kEps || std::fabs(to.y - pos_.y) > kEps;
        if (across_move && limits_.enforce && limits_.keep_clear && z > limits_.safe_z + kEps) {
            out.push_back(order(0, 0, limits_.safe_z - z));
            z = limits_.safe_z;
        }
        if (across_move) out.push_back(order(to.x - pos_.x, to.y - pos_.y, 0));
        if (std::fabs(to.z - z) > kEps) out.push_back(order(0, 0, to.z - z));
        if (out.empty()) return JogRefusal::None;
        pos_ = to;
        touched_ = true;
        return JogRefusal::None;
    }

    /// All the axes go to the machine zero: across (after Z goes up, when the head is too low for that), then Z up.
    JogRefusal home(const JogGate& gate, std::vector<JogOrder>& out) { return goto_pos(JogPos{}, gate, out); }

    // ---- the four places
    /// Keeps the place of the head in place `i` (0..3).
    bool save_place(std::size_t i) {
        if (i >= kPlaces) return false;
        places_[i] = pos_;
        has_place_[i] = true;
        return true;
    }
    bool has_place(std::size_t i) const { return i < kPlaces && has_place_[i]; }
    JogPos place(std::size_t i) const { return i < kPlaces ? places_[i] : JogPos{}; }
    /// Goes to the place kept in `i`; nothing happens (and nothing is refused but the gate) for an empty place.
    JogRefusal goto_place(std::size_t i, const JogGate& gate, std::vector<JogOrder>& out) {
        if (!has_place(i)) {
            out.clear();
            return check(gate);
        }
        return goto_pos(places_[i], gate, out);
    }

private:
    static bool across(JogDir d) { return d != JogDir::ZUp && d != JogDir::ZDown; }
    static void unit(JogDir d, double& x, double& y, double& z) {
        x = y = z = 0;
        switch (d) {
            case JogDir::XMinus: x = -1; break;
            case JogDir::XPlus: x = 1; break;
            case JogDir::YMinus: y = -1; break;
            case JogDir::YPlus: y = 1; break;
            case JogDir::ZUp: z = -1; break;
            case JogDir::ZDown: z = 1; break;
        }
    }

    std::size_t nearest(double v) const {
        std::size_t best = 0;
        for (std::size_t i = 1; i < steps_.size(); ++i)
            if (std::fabs(steps_[i] - v) < std::fabs(steps_[best] - v)) best = i;
        return best;
    }

    // the emergency stop always blocks; a running job blocks unless the switch `jog_block_running` was turned off
    JogRefusal check(const JogGate& g) const {
        if (g.estop) return JogRefusal::Estop;
        if (g.job_running && block_running_) return JogRefusal::Job;
        return JogRefusal::None;
    }

    double chunk_s() const { return std::min(chunk_mm_, max_mm_) / std::max(feed() / 60.0, kEps); }

    JogOrder order(double dx, double dy, double dz) const { return JogOrder{dx, dy, dz, feed()}; }

    JogPos clamp(JogPos p) const {
        return JogPos{jog_clamp(p.x, 0, limits_.x_max), jog_clamp(p.y, 0, limits_.y_max), jog_clamp(p.z, 0, limits_.z_floor)};
    }

    /// Moves the head `dist` mm along `dir`, as far as the travel allows; the distance left over is not moved.
    JogRefusal go(JogDir dir, double dist, JogOrder& o) {
        double ux, uy, uz;
        unit(dir, ux, uy, uz);
        if (across(dir) && limits_.enforce && limits_.keep_clear && pos_.z > limits_.safe_z + kEps) return JogRefusal::RaiseZ;
        const JogPos want{pos_.x + ux * dist, pos_.y + uy * dist, pos_.z + uz * dist};
        const JogPos to = limits_.enforce ? clamp(want) : want;
        // a move that the travel does not cut is the distance asked for, not a difference of two sums
        const bool cut = !(to == want);
        const double dx = cut ? to.x - pos_.x : ux * dist, dy = cut ? to.y - pos_.y : uy * dist, dz = cut ? to.z - pos_.z : uz * dist;
        if (std::fabs(dx) + std::fabs(dy) + std::fabs(dz) < kEps) return JogRefusal::Limit;
        pos_ = to;
        touched_ = true;
        o = order(dx, dy, dz);
        return JogRefusal::None;
    }

    JogLimits limits_;
    std::vector<double> steps_;
    double max_mm_ = 20, chunk_mm_ = 2;
    double feeds_[3] = {300, 1200, 3000};
    bool block_running_ = true;
    bool applied_ = false;
    std::size_t step_ = 0;
    bool continuous_ = false;
    JogSpeed speed_ = JogSpeed::Medium;
    JogPos pos_;
    bool held_ = false;
    JogDir dir_ = JogDir::XPlus;
    double owed_s_ = 0;
    bool touched_ = false;
    JogPos places_[kPlaces];
    bool has_place_[kPlaces] = {false, false, false, false};
};

/// The `what` of the action the gate asks about for a press: `jog X -`, `jog X +`... with the distance (`jog X +1 mm`), as `order_text` of status.hpp writes it.
inline std::string jog_order_id(JogDir d, double step_mm, bool continuous) {
    const char* axis = (d == JogDir::XMinus || d == JogDir::XPlus) ? "X" : ((d == JogDir::YMinus || d == JogDir::YPlus) ? "Y" : "Z");
    // the sign is the way of the arrow on the machine (Z up is +, as the arrows of the screen), not the depth
    const char* sign = (d == JogDir::XMinus || d == JogDir::YMinus || d == JogDir::ZDown) ? "-" : "+";
    char b[64];
    if (continuous) std::snprintf(b, sizeof b, "jog %s %s", axis, sign);
    else std::snprintf(b, sizeof b, "jog %s %s%g mm", axis, sign, step_mm);
    return b;
}

/// Reads a `jog ...` id back: the direction and, when it has one, the distance. False for anything else.
inline bool parse_jog_id(const std::string& what, JogDir& d, bool& has_step) {
    if (what.size() < 7 || what.compare(0, 4, "jog ") != 0) return false;
    const char axis = what[4];
    const char sign = what[6];
    if ((axis != 'X' && axis != 'Y' && axis != 'Z') || what[5] != ' ' || (sign != '+' && sign != '-')) return false;
    if (axis == 'X') d = sign == '+' ? JogDir::XPlus : JogDir::XMinus;
    else if (axis == 'Y') d = sign == '+' ? JogDir::YPlus : JogDir::YMinus;
    else d = sign == '+' ? JogDir::ZUp : JogDir::ZDown;
    has_step = what.size() > 7;
    return true;
}

/// The id of the order that sends the head home, and the one that sends it to a place kept (`jog goto 2` is the place P2).
inline std::string jog_home_id() { return "jog home"; }
inline std::string jog_goto_id(int place) { return "jog goto " + std::to_string(place + 1); }

/// What a confirmed `jog ...` action asks for.
enum class JogCommand { None, Press, Home, Goto };
struct JogAsk {
    JogCommand command = JogCommand::None;
    JogDir dir = JogDir::XPlus;
    int place = 0;  // Goto: 0..3
};

inline JogAsk parse_jog_command(const std::string& what) {
    JogAsk a;
    if (what == "jog home") {
        a.command = JogCommand::Home;
        return a;
    }
    if (what.compare(0, 9, "jog goto ") == 0 && what.size() == 10 && what[9] >= '1' && what[9] <= '4') {
        a.command = JogCommand::Goto;
        a.place = what[9] - '1';
        return a;
    }
    bool has_step = false;
    if (parse_jog_id(what, a.dir, has_step)) a.command = JogCommand::Press;
    return a;
}

}  // namespace hmi
