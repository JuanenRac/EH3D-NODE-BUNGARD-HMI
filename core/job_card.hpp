// EH3D-NODE-BUNGARD-HMI - core/job_card.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// The card of the work in hand, the way the desktop program shows it under the bar of progress: four blocks. 1) the tool and its step (number, diameter, place in the bar, hole
// or cut X of N); 2) the times (left, elapsed and the local hour the job is expected to end at); 3) the position and the depth (X, Y, Z, the depth of the stroke, the feed and
// the turns of the spindle); 4) the counters (holes done or length routed, tool changes). Everything the server does not say is a dash, never an invented number. The numbers are
// worked out from the bar of the job and from what the fleet reports, so it is tested on a computer.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "job_bar.hpp"
#include "server_link.hpp"
#include "ui_text.hpp"

namespace hmi {

/// What the card holds, with a negative number (or `false`) for what is not known.
struct JobCard {
    bool known = false;  // there is a job with steps to show
    // 1) the tool and its step
    int tool = 0;
    double diameter_mm = 0;
    int slot = 0;        // the place in the bar of the tool at work, 0: not known
    int step = 0, steps = 0;
    bool route = false;
    // 2) the times
    long left_s = -1, elapsed_s = -1;
    bool end_known = false;
    int end_hour = 0, end_minute = 0;
    // 3) the position and the depth
    bool pos_known = false;
    double x = 0, y = 0, z = 0;
    double depth_mm = -1, feed = 0, rpm = 0;
    // 4) the counters
    int holes_done = 0, holes_total = 0;
    double routed_mm = -1;
    int changes = -1;
};

/// The wall clock is believed only when it is after this moment (a chip that has not heard the time is at 1970).
constexpr long kClockSetAfter = 1577836800;  // the first of January of 2020, in seconds since 1970

/// The card of the job. `bar` is the bar of the work in hand, `row` the machine as the fleet reports it (null when it is not in the fleet), `now_epoch_s` the wall clock in UTC
/// (0 or before `kClockSetAfter`: not known), `utc_offset_min` the local time zone, `slot` the place in the bar that the screen gave the tool at work (the fleet's own number,
/// when it says one, wins).
inline JobCard make_job_card(const JobBar& bar, const MachineRow* row, bool route, long now_epoch_s, int utc_offset_min, int slot) {
    JobCard c;
    if (!bar.known) return c;
    c.known = true;
    c.route = route;
    c.holes_done = bar.steps_done;
    c.holes_total = bar.steps_total;
    if (bar.current >= 0) {
        const ToolProgress& t = bar.tools[static_cast<std::size_t>(bar.current)];
        c.tool = t.number;
        c.diameter_mm = t.diameter_mm;
        c.step = t.done < t.total ? t.done + 1 : t.total;  // the step being done
        c.steps = t.total;
    }
    c.slot = (row != nullptr && row->tool_slot > 0) ? row->tool_slot : slot;
    c.left_s = bar.seconds_left;
    if (row != nullptr && row->elapsed_s >= 0) c.elapsed_s = std::lround(row->elapsed_s);
    else if (row != nullptr && row->estimate_s > 0 && bar.seconds_left >= 0) c.elapsed_s = std::max(0L, std::lround(row->estimate_s) - bar.seconds_left);
    if (c.left_s >= 0 && now_epoch_s >= kClockSetAfter) {
        long local = now_epoch_s + c.left_s + static_cast<long>(utc_offset_min) * 60;
        local %= 86400;
        if (local < 0) local += 86400;
        c.end_known = true;
        c.end_hour = static_cast<int>(local / 3600);
        c.end_minute = static_cast<int>(local / 60 % 60);
    }
    if (row != nullptr) {
        c.pos_known = row->pos_known;
        c.x = row->pos_x;
        c.y = row->pos_y;
        c.z = row->pos_z;
        c.depth_mm = row->depth_mm;
        c.feed = row->feed_mm_min;
        c.rpm = row->spindle_rpm;
        c.routed_mm = row->routed_mm;
        c.changes = row->tool_changes;
    }
    return c;
}

/// `07:45`
inline std::string hour_text(int hour, int minute) {
    char b[8];
    std::snprintf(b, sizeof b, "%02d:%02d", hour, minute);
    return b;
}

namespace detail {
inline std::string number_or_dash(bool known, const char* format, double v) {
    if (!known) return "-";
    char b[32];
    std::snprintf(b, sizeof b, format, v);
    return b;
}
}  // namespace detail

/// The four blocks of the card as lines of text: [0] the tool and its step, [1] the times, [2] the position and the depth, [3] the counters. A card that is not known has four
/// empty blocks.
struct CardText {
    std::string block[4];
};

inline CardText card_text(const JobCard& c, Lang l) {
    CardText t;
    if (!c.known) return t;
    char b[160];
    // 1) the tool and its step
    if (c.tool > 0) {
        std::string tool = std::string(text(T::Tool, l)) + " " + std::to_string(c.tool);
        if (c.diameter_mm > 0) {
            std::snprintf(b, sizeof b, "  %.2f mm", c.diameter_mm);
            tool += b;
        }
        if (c.slot > 0) tool += "  @" + std::to_string(c.slot);
        std::snprintf(b, sizeof b, "%d/%d %s", c.step, c.steps, text(c.route ? T::Cuts : T::Holes, l));
        t.block[0] = tool + "\n" + b;
    }
    // 2) the times
    t.block[1] = std::string(text(T::TimeLeft, l)) + " " + (c.left_s >= 0 ? clock_text(c.left_s) : "-");
    t.block[1] += std::string("\n") + text(T::Elapsed, l) + " " + (c.elapsed_s >= 0 ? clock_text(c.elapsed_s) : "-");
    t.block[1] += std::string("\n") + text(T::EndsAt, l) + " " + (c.end_known ? hour_text(c.end_hour, c.end_minute) : "-");
    // 3) the position and the depth
    t.block[2] = "X " + detail::number_or_dash(c.pos_known, "%.2f", c.x) + "  Y " + detail::number_or_dash(c.pos_known, "%.2f", c.y);
    t.block[2] += "\nZ " + detail::number_or_dash(c.pos_known, "%.2f", c.z);
    if (c.depth_mm >= 0) t.block[2] += "  (" + detail::number_or_dash(true, "%.2f", c.depth_mm) + ")";
    t.block[2] += std::string("\n") + text(T::Feed, l) + " " + (c.feed > 0 ? std::to_string(std::lround(c.feed)) : std::string("-")) + "  " + (c.rpm > 0 ? std::to_string(std::lround(c.rpm)) : std::string("-")) + " rpm";
    // 4) the counters
    std::snprintf(b, sizeof b, "%s %d/%d", text(T::Holes, l), c.holes_done, c.holes_total);
    t.block[3] = c.route ? std::string(text(T::Routed, l)) + " " + detail::number_or_dash(c.routed_mm >= 0, "%.1f", c.routed_mm) + " mm" : std::string(b);
    t.block[3] += std::string("\n") + text(T::Changes, l) + " " + (c.changes >= 0 ? std::to_string(c.changes) : std::string("-"));
    return t;
}

}  // namespace hmi
