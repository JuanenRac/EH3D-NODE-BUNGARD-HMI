// EH3D-NODE-BUNGARD-HMI - main/ui.cpp: the screens of the touch display, in the order and with the names of the desktop program (machine,
// job, viewer, machines and stations, settings). The stop button is always on top and always works. This has not been run on a module
// yet. GPL-3.0-or-later.
#include "ui.hpp"

#include "lvgl.h"

#include "../core/ui_text.hpp"

namespace ui {

namespace {

void (*g_post)(const Request&) = nullptr;
lv_obj_t* g_banner = nullptr;
lv_obj_t* g_armed = nullptr;
lv_obj_t* g_ack = nullptr;
lv_obj_t* g_dialog = nullptr;
lv_obj_t* g_dialog_text = nullptr;
lv_obj_t* g_note = nullptr;

void send(const Request& r) {
    if (g_post != nullptr) g_post(r);
}

void on_stop(lv_event_t*) {
    Request r;
    r.action = {hmi::Kind::Stop, "stop"};
    send(r);
}

void on_ack(lv_event_t*) {
    Request r;
    r.release_acknowledged = true;
    send(r);
}

void on_confirm(lv_event_t*) {
    Request r;
    r.confirmed = true;
    send(r);
}

void on_cancel(lv_event_t*) {
    Request r;
    r.cancelled = true;
    send(r);
}

// a button that asks for a move: the user data is a static action
void on_action(lv_event_t* e) {
    Request r;
    r.action = *static_cast<const hmi::Action*>(lv_event_get_user_data(e));
    send(r);
}

lv_obj_t* button(lv_obj_t* parent, const char* label, lv_event_cb_t cb, const void* user, lv_color_t colour) {
    lv_obj_t* b = lv_button_create(parent);
    lv_obj_set_style_bg_color(b, colour, 0);
    lv_obj_set_size(b, 220, 110);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, const_cast<void*>(user));
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, label);
    lv_obj_center(l);
    return b;
}

const hmi::Action kJogXp{hmi::Kind::Motion, "jog X +"};
const hmi::Action kJogXm{hmi::Kind::Motion, "jog X -"};
const hmi::Action kJogYp{hmi::Kind::Motion, "jog Y +"};
const hmi::Action kJogYm{hmi::Kind::Motion, "jog Y -"};
const hmi::Action kJogZp{hmi::Kind::Motion, "jog Z +"};
const hmi::Action kJogZm{hmi::Kind::Motion, "jog Z -"};
const hmi::Action kStart{hmi::Kind::Motion, "start job"};
const hmi::Action kHome{hmi::Kind::Motion, "home"};
const hmi::Action kSpindle{hmi::Kind::Spindle, "spindle on"};

}  // namespace

void build(const hmi::Settings& s, void (*post)(const Request&)) {
    using hmi::T;
    g_post = post;
    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x15110c), 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);

    // the top: the stop button, the state and the acknowledgement of a released hardware stop
    lv_obj_t* top = lv_obj_create(scr);
    lv_obj_set_size(top, LV_PCT(100), 130);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    button(top, hmi::text(T::Stop, s.language), on_stop, nullptr, lv_color_hex(0xc62828));
    g_armed = lv_label_create(top);
    g_banner = lv_label_create(top);
    lv_obj_set_style_text_color(g_banner, lv_color_hex(0xff5252), 0);
    g_ack = button(top, hmi::text(T::AckRelease, s.language), on_ack, nullptr, lv_color_hex(0xf9a825));
    lv_obj_add_flag(g_ack, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* tabs = lv_tabview_create(scr);
    lv_obj_set_size(tabs, LV_PCT(100), LV_PCT(80));
    lv_obj_t* machine = lv_tabview_add_tab(tabs, hmi::text(T::Machine, s.language));
    lv_obj_t* job = lv_tabview_add_tab(tabs, hmi::text(T::Job, s.language));
    lv_tabview_add_tab(tabs, hmi::text(T::Viewer, s.language));
    lv_tabview_add_tab(tabs, hmi::text(T::Fleet, s.language));
    lv_tabview_add_tab(tabs, hmi::text(T::Settings, s.language));

    lv_obj_set_flex_flow(machine, LV_FLEX_FLOW_ROW_WRAP);
    static char labels[6][40];
    const struct {
        T t;
        const hmi::Action* a;
        const char* sign;
    } jog[] = {
        {T::JogX, &kJogXm, "-"}, {T::JogX, &kJogXp, "+"}, {T::JogY, &kJogYm, "-"},
        {T::JogY, &kJogYp, "+"}, {T::JogZ, &kJogZm, "-"}, {T::JogZ, &kJogZp, "+"},
    };
    for (int i = 0; i < 6; ++i) {
        lv_snprintf(labels[i], sizeof labels[i], "%s %s", hmi::text(jog[i].t, s.language), jog[i].sign);
        button(machine, labels[i], on_action, jog[i].a, lv_color_hex(0x37474f));
    }
    button(machine, hmi::text(T::Home, s.language), on_action, &kHome, lv_color_hex(0x37474f));
    button(machine, hmi::text(T::Spindle, s.language), on_action, &kSpindle, lv_color_hex(0x37474f));
    lv_obj_set_flex_flow(job, LV_FLEX_FLOW_ROW_WRAP);
    button(job, hmi::text(T::Start, s.language), on_action, &kStart, lv_color_hex(0x2e7d32));

    g_note = lv_label_create(scr);
    lv_obj_set_style_text_color(g_note, lv_color_hex(0xffb300), 0);
    lv_label_set_text(g_note, "");

    // the question of the gate: a move is done only after this
    g_dialog = lv_obj_create(scr);
    lv_obj_add_flag(g_dialog, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(g_dialog, 640, 340);
    lv_obj_center(g_dialog);
    g_dialog_text = lv_label_create(g_dialog);
    lv_obj_align(g_dialog_text, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_t* yes = button(g_dialog, hmi::text(T::Confirm, s.language), on_confirm, nullptr, lv_color_hex(0x2e7d32));
    lv_obj_align(yes, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_t* no = button(g_dialog, hmi::text(T::Cancel, s.language), on_cancel, nullptr, lv_color_hex(0x546e7a));
    lv_obj_align(no, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_add_flag(g_dialog, LV_OBJ_FLAG_HIDDEN);
}

void show(const State& st, const hmi::Settings& s) {
    using hmi::T;
    lv_label_set_text(g_armed, hmi::text(st.armed ? T::Armed : T::NotArmed, s.language));
    lv_label_set_text(g_banner, st.stop_pressed ? hmi::text(T::StopPressed, s.language) : "");
    if (st.stop_latched && !st.stop_pressed) {
        lv_obj_remove_flag(g_ack, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g_ack, LV_OBJ_FLAG_HIDDEN);
    }
}

void ask(const hmi::Action* action, const hmi::Settings& s) {
    using hmi::T;
    if (action == nullptr) {
        lv_obj_add_flag(g_dialog, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    static char text[96];
    lv_snprintf(text, sizeof text, "%s\n%s", hmi::text(action->kind == hmi::Kind::Spindle ? T::AskSpindle : T::AskMove, s.language),
                action->what.c_str());
    lv_label_set_text(g_dialog_text, text);
    lv_obj_remove_flag(g_dialog, LV_OBJ_FLAG_HIDDEN);
}

void refused(hmi::Why why, const hmi::Settings& s) { lv_label_set_text(g_note, hmi::why_text(why, s.language)); }

}  // namespace ui
