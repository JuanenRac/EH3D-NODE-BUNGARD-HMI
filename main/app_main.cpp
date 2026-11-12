// EH3D-NODE-BUNGARD-HMI - main/app_main.cpp: reads the settings, starts the module, and runs every request of the operator, from a touch or
// from the voice, through the gate (core/gate.hpp). A second task is the client of EH3D-SERVER: it reads the fleet every few seconds and
// sends what the operator asked (a job made of a file of the microSD card, its start, its confirmation), so the desktop programs and the
// node see the same jobs at the same time. This has not been built or run on a module yet. GPL-3.0-or-later.
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "../core/hmi_config.hpp"
#include "../core/job_card.hpp"
#include "../core/job_memory.hpp"
#include "../core/jog_panel.hpp"
#include "../core/orientation.hpp"
#include "../core/quality.hpp"
#include "../core/sdfiles.hpp"
#include "../core/stage_notice.hpp"
#include "../core/status.hpp"
#include "../core/ui_text.hpp"
#include "../core/voice.hpp"
#include "../core/wav.hpp"
#include "../core/workflow.hpp"
#include "board.hpp"
#include "image_confirm.hpp"
#include "server_link.hpp"
#include "ui.hpp"

static const char* TAG = "hmi";
static hmi::Settings g_settings;
static QueueHandle_t g_requests = nullptr;  // from the screens to the application task: ui::Request
static QueueHandle_t g_to_server = nullptr; // from the application task to the link task: ServerJob
static QueueHandle_t g_say = nullptr;       // the notices to say: hmi::Cue
static SemaphoreHandle_t g_lock = nullptr;  // guards what the link task shares: the fleet, the note, the workflow
static TaskHandle_t g_stop_task = nullptr;  // the task that sends a stop to the server, apart from the queue of the link
static TaskHandle_t g_say_task = nullptr;   // the task of the notices
static TaskHandle_t g_link_task = nullptr;  // the task of the link
static std::vector<hmi::Request> g_stop_reqs;  // what it sends (guarded by g_lock)

// what the link task is asked to do
struct ServerJob {
    enum class Do { Create, Start, Confirm, ResumeFrom, Pause, Release, Light, Inspect, Preset, Calibrate, Last } what = Do::Create;
};

static hmi::FleetView g_fleet;
static bool g_fleet_new = false;
static bool g_server_ok = false;
static hmi::LinkTracker g_link;                    // the state of the link and the age of the data (guarded by g_lock)
static hmi::Link g_link_said = hmi::Link::Unknown; // the state the operator was last told about
static hmi::Answer g_answer_said = hmi::Answer::Fresh;
static std::string g_version;                      // the version of this firmware, shown on the screen
static std::vector<hmi::Problem> g_problems;       // the lines of hmi.cfg that were not used
static bool g_cfg_missing = false;                 // hmi.cfg is not there
static std::string g_note;
static hmi::Workflow g_flow;
static hmi::SpindleView g_spindle;  // the spindle of the machine, as the server says it (asked now and then)
static bool g_spindle_new = false;
static hmi::TableSize g_table;  // the table of the machine, as the server says it (asked with the spindle)
static hmi::JogPanel* g_jog = nullptr;  // the panel that moves the machine by hand (only the application task touches it)
static std::string g_jog_note;          // why the last press did not move the machine
static bool g_finger_down = false;      // an arrow of the panel is under a finger now
static bool g_estop_now = true;         // the hardware stop is pressed or not yet acknowledged (set by the application loop)
static hmi::EndWatcher g_end;           // tells once when a stage is done or the whole job is over

// the quality check: what the server last said, the ring of LEDs, and what the operator chose for the next request
static hmi::QualityView g_quality;
static hmi::LightView g_light;
static bool g_quality_new = false;
static bool g_quality_running = false;
static int g_arg_light = 0;
static int g_arg_hole = 0;
static int g_arg_preset = 0;


// ------------------------------------------------------------------------------------------------------------------------------ the job that this screen follows
// The job lives in the server. The screen keeps only which one is its own (its id, the machine, its name) in the flash, from the moment the operator starts it until it
// is over, so that after a power cut or a reset it follows the same job again: the next reading of the fleet says if it still runs, waits for the operator or is over.

static const char* kMemoryKey = "job";
static bool g_remembered = false;

static void memory_save(const hmi::JobMemory& m) {
    nvs_handle_t h;
    if (nvs_open("eh3d_hmi", NVS_READWRITE, &h) != ESP_OK) return;
    if (nvs_set_str(h, kMemoryKey, hmi::memory_text(m).c_str()) == ESP_OK) nvs_commit(h);
    nvs_close(h);
    g_remembered = true;
}

static void memory_clear() {
    nvs_handle_t h;
    if (nvs_open("eh3d_hmi", NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_key(h, kMemoryKey);
        nvs_commit(h);
        nvs_close(h);
    }
    g_remembered = false;
}

static bool memory_load(hmi::JobMemory& m) {
    nvs_handle_t h;
    if (nvs_open("eh3d_hmi", NVS_READONLY, &h) != ESP_OK) return false;
    std::size_t n = 0;
    bool ok = false;
    if (nvs_get_str(h, kMemoryKey, nullptr, &n) == ESP_OK && n > 1 && n < 512) {
        std::string text(n, '\0');
        if (nvs_get_str(h, kMemoryKey, &text[0], &n) == ESP_OK) {
            text.resize(std::strlen(text.c_str()));
            ok = hmi::memory_parse(text, m);
        }
    }
    nvs_close(h);
    return ok;
}

// ------------------------------------------------------------------------------------------------------------------------------ the notices
// A notice is off, the tone of the screen or a recorded voice (voices/LANG/f|m/<name>.wav on the card); a voice that is not there, or is not 16-bit mono, plays the tone instead of
// nothing. They are said by a task of their own, because playing takes a second or two and the screen and the gate must not wait for it.

static void say_task(void*) {
    for (;;) {
        hmi::Cue cue;
        if (xQueueReceive(g_say, &cue, portMAX_DELAY) != pdTRUE) continue;
        const hmi::NoticeSetting* setting = nullptr;
        switch (cue) {
            case hmi::Cue::Finished: setting = &g_settings.sound_finish; break;
            case hmi::Cue::ToolChange: setting = &g_settings.sound_tool_change; break;
            case hmi::Cue::CheckOk: setting = &g_settings.sound_check_ok; break;
            case hmi::Cue::CheckNotOk: setting = &g_settings.sound_check_bad; break;
            case hmi::Cue::Stage: setting = &g_settings.sound_stage; break;
        }
        if (setting == nullptr || setting->kind == hmi::NoticeSetting::Kind::Off) continue;
        const unsigned volume = g_settings.volume_percent;
        if (setting->kind == hmi::NoticeSetting::Kind::Voice) {
            std::string bytes;
            hmi::WavInfo info;
            if (board::read_file(hmi::notice_file("/sdcard", *setting, cue), bytes, 400 * 1024) && hmi::parse_wav(bytes, info)) {
                std::vector<std::int16_t> samples = hmi::wav_samples(bytes, info);
                hmi::scale_samples(samples, volume);
                board::play(samples.data(), samples.size(), info.rate);
                continue;
            }
        }
        const std::vector<std::int16_t> tone = hmi::tone_samples(cue, 22050, volume);
        board::play(tone.data(), tone.size(), 22050);
    }
}

static void say(hmi::Cue cue) {
    if (g_say != nullptr) xQueueSend(g_say, &cue, 0);
}

// called by the screens in the LVGL task: the request is only queued
static void post(const ui::Request& r) {
    if (g_requests != nullptr) xQueueSend(g_requests, &r, 0);
}

static std::string read_file(const char* path) {
    std::string text;
    if (FILE* f = std::fopen(path, "rb")) {
        char buf[256];
        std::size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
        std::fclose(f);
    }
    return text;
}

static void set_note(const std::string& s) {
    xSemaphoreTake(g_lock, portMAX_DELAY);
    g_note = s;
    xSemaphoreGive(g_lock);
}

// A note in the language of the operator (the texts are in core/status.hpp, never literals here).
static void note(hmi::ScreenNote n, const std::string& detail = "") { set_note(hmi::note_with(n, detail, g_settings.language)); }
static std::string said(hmi::CallError e) { return hmi::call_error_text(e, g_settings, g_settings.language); }

// What the screen says about hmi.cfg: it is missing, or the first line that could not be used and why.
static std::string config_issue() {
    if (g_cfg_missing) return hmi::text(hmi::T::CfgNoFile, g_settings.language);
    return hmi::cfg_report(g_problems, g_settings.language);
}

// A reading of the fleet came back as `answer`: the link tracker takes it, and the operator is told once when the link changes into a state that is not connected (this is
// also the check of the server at the start) or when the server refuses with its own error.
static void link_update(hmi::Answer answer, bool machine_found, const hmi::Response& r) {
    xSemaphoreTake(g_lock, portMAX_DELAY);
    g_link.update(answer, machine_found, board::now_ms());
    const hmi::Link now = g_link.state();
    const bool changed = now != g_link_said;
    const bool refused = answer == hmi::Answer::ServerError && g_answer_said != hmi::Answer::ServerError;
    g_link_said = now;
    g_answer_said = answer;
    xSemaphoreGive(g_lock);
    if (changed && now != hmi::Link::Connected) set_note(hmi::link_note(now, g_settings, g_settings.language));
    else if (refused) set_note(hmi::error_line(r));
}

// A request of the quality check: the ring of LEDs, a check, a preset of the profile, the calibration of the scale, or the last check. Everything goes through the server of
// the machine, which holds the token of the node of the drill head.
static void quality_job(const ServerJob& job) {
    if (g_settings.server_machine.empty()) {
        set_note(hmi::quality_text(hmi::Q::NeedServer, g_settings.language));
        return;
    }
    int light = 0, hole = 0, preset = 0;
    xSemaphoreTake(g_lock, portMAX_DELAY);
    light = g_arg_light;
    hole = g_arg_hole;
    preset = g_arg_preset;
    xSemaphoreGive(g_lock);
    hmi::Request req;
    switch (job.what) {
        case ServerJob::Do::Light: req = hmi::light_request(g_settings.server_machine, hmi::kLightLevels[light]); break;
        case ServerJob::Do::Inspect: req = hmi::inspect_request(g_settings.server_machine, hmi::kLightLevels[light], hmi::kHoleWidths[hole]); break;
        case ServerJob::Do::Preset: req = hmi::preset_request(g_settings.server_machine, preset); break;
        case ServerJob::Do::Calibrate: req = hmi::calibrate_request(g_settings.server_machine); break;
        case ServerJob::Do::Last: req = hmi::inspect_last_request(g_settings.server_machine); break;
        default: return;
    }
    hmi::Response r;
    hmi::CallError error = hmi::CallError::None;
    const bool reached = srvlink::call(g_settings, req, r, error);
    xSemaphoreTake(g_lock, portMAX_DELAY);
    if (job.what == ServerJob::Do::Inspect) g_quality_running = false;
    bool say_ok = false, say_bad = false;
    if (!reached) {
        g_note = said(error);
    } else if (r.status == 200) {
        hmi::QualityView q;
        hmi::LightView l;
        if ((job.what == ServerJob::Do::Inspect || job.what == ServerJob::Do::Last) && hmi::parse_quality(r.body, q)) {
            g_quality = q;
            // only a check that was just made is said, never the last one that is shown when the page opens; "cannot judge" is neither correct nor not correct
            if (job.what == ServerJob::Do::Inspect) {
                say_ok = q.verdict == hmi::QualityVerdict::Ok;
                say_bad = q.verdict == hmi::QualityVerdict::NotOk;
            }
        }
        if (job.what == ServerJob::Do::Light && hmi::parse_light(r.body, l)) g_light = l;
        if (job.what == ServerJob::Do::Preset || job.what == ServerJob::Do::Calibrate) g_note = hmi::note_text(hmi::ScreenNote::Ok, g_settings.language);
        g_quality_new = true;
    } else if (job.what != ServerJob::Do::Last) {
        g_note = hmi::error_line(r);  // the screen that has never been checked has no last check: that is not an error to show
    }
    xSemaphoreGive(g_lock);
    if (say_ok) say(hmi::Cue::CheckOk);
    if (say_bad) say(hmi::Cue::CheckNotOk);
}

// The stop does not wait in the queue of the link, behind a file being sent or a reading of the fleet: it has a task of its own, short timeouts and three tries, and when the
// server cannot be reached the note says so, so the operator knows to use the hardware stop. It cancels the job the fleet shows on the machine, whoever started it.
static void stop_task(void*) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        std::vector<hmi::Request> reqs;
        xSemaphoreTake(g_lock, portMAX_DELAY);
        reqs.swap(g_stop_reqs);
        xSemaphoreGive(g_lock);
        for (const hmi::Request& req : reqs) {
            hmi::Response r;
            hmi::CallError error = hmi::CallError::None;
            bool reached = false;
            for (int attempt = 0; attempt < 3 && !reached; ++attempt) reached = srvlink::call(g_settings, req, r, error, 1500);
            if (!reached) note(hmi::ScreenNote::StopFail, said(error));
            else if (r.status != 200 && r.status != 202 && r.status != 409) note(hmi::ScreenNote::StopRefused, hmi::error_line(r));  // 409: the job was no longer running
            else note(hmi::ScreenNote::StopSent);
        }
    }
}

// A stop from the touch screen, the voice or the hardware input: hands the cancel of the job of the machine to the stop task at once.
static void request_stop() {
    xSemaphoreTake(g_lock, portMAX_DELAY);
    g_stop_reqs = g_flow.stop_requests(g_fleet, g_settings.server_machine);
    const bool none = g_stop_reqs.empty();
    xSemaphoreGive(g_lock);
    if (none) note(hmi::ScreenNote::NoJobToStop);
    else if (g_stop_task != nullptr) xTaskNotifyGive(g_stop_task);
}

// ------------------------------------------------------------------------------------------------------------------------------ the panel that moves the machine by hand
static const hmi::JogDir kJogDirs[6] = {hmi::JogDir::XMinus, hmi::JogDir::XPlus, hmi::JogDir::YMinus, hmi::JogDir::YPlus, hmi::JogDir::ZDown, hmi::JogDir::ZUp};

static void refresh_jog() {
    if (g_jog == nullptr) return;
    ui::JogView v;
    v.steps = g_jog->steps();
    v.step = static_cast<int>(g_jog->step_index());
    v.continuous = g_jog->continuous();
    v.speed = g_jog->speed() == hmi::JogSpeed::Slow ? 0 : (g_jog->speed() == hmi::JogSpeed::Medium ? 1 : 2);
    v.feeds[0] = g_jog->feed_of(hmi::JogSpeed::Slow);
    v.feeds[1] = g_jog->feed_of(hmi::JogSpeed::Medium);
    v.feeds[2] = g_jog->feed_of(hmi::JogSpeed::Fast);
    for (int i = 0; i < 4; ++i) v.has_place[i] = g_jog->has_place(static_cast<std::size_t>(i));
    // the head is where the panel put it, and only after it moved it (the machine does not report its position here)
    v.pos_known = g_jog->touched();
    v.x = g_jog->pos().x;
    v.y = g_jog->pos().y;
    v.z = g_jog->pos().z;
    v.note = g_jog_note;
    ui::show_jog(v, g_settings);
}

static const char* jog_refusal_text(hmi::JogRefusal r) {
    switch (r) {
        case hmi::JogRefusal::Job: return hmi::why_text(hmi::Why::JobRunning, g_settings.language);
        case hmi::JogRefusal::Estop: return hmi::why_text(hmi::Why::HardwareStop, g_settings.language);
        case hmi::JogRefusal::Limit: return hmi::text(hmi::T::JogLimit, g_settings.language);
        case hmi::JogRefusal::RaiseZ: return hmi::text(hmi::T::JogRaise, g_settings.language);
        case hmi::JogRefusal::None: break;
    }
    return "";
}

// The moves of the panel go to the machine through the link to the node, which is not written yet: nothing is sent, so the position the panel keeps is not changed either.
static bool send_jog_orders(const std::vector<hmi::JogOrder>& orders) {
    for (const hmi::JogOrder& o : orders) ESP_LOGI(TAG, "jog %.3f %.3f %.3f at %.0f (not sent: no link to the node)", o.dx, o.dy, o.dz, o.feed);
    return false;
}

// A confirmed order of the panel: the moves it makes are worked out on a copy, and the copy is kept only if the moves went.
static void run_jog(const hmi::Action& go) {
    const hmi::JogAsk ask = hmi::parse_jog_command(go.what);
    if (g_jog == nullptr || ask.command == hmi::JogCommand::None) {
        note(hmi::ScreenNote::MoveNoLink);
        return;
    }
    bool running = false;
    hmi::TableSize table;
    xSemaphoreTake(g_lock, portMAX_DELAY);
    running = g_flow.stage() == hmi::Stage::Running || g_flow.stage() == hmi::Stage::Waiting;
    table = g_table.known() ? g_table : hmi::TableSize{static_cast<double>(g_settings.table_width_mm), static_cast<double>(g_settings.table_depth_mm)};
    xSemaphoreGive(g_lock);
    g_jog->set_limits(hmi::make_jog_limits(table.width, table.depth, g_settings));
    hmi::JogPanel trial = *g_jog;
    const bool mode = trial.continuous();
    const hmi::JogGate jg{running, g_estop_now};
    std::vector<hmi::JogOrder> orders;
    hmi::JogRefusal r = hmi::JogRefusal::None;
    if (ask.command == hmi::JogCommand::Press) {
        // an arrow already let go before the confirmation cannot be held: it makes one step
        if (mode && !g_finger_down) trial.set_continuous(false);
        r = trial.press(ask.dir, jg, orders);
        trial.set_continuous(mode);
    } else if (ask.command == hmi::JogCommand::Home) {
        r = trial.home(jg, orders);
    } else {
        r = trial.goto_place(static_cast<std::size_t>(ask.place), jg, orders);
    }
    if (r != hmi::JogRefusal::None) {
        g_jog_note = jog_refusal_text(r);
        refresh_jog();
        return;
    }
    if (!orders.empty() && !send_jog_orders(orders)) {
        g_jog_note = hmi::note_with(hmi::ScreenNote::MoveNoLink, "", g_settings.language);
        refresh_jog();
        return;
    }
    *g_jog = trial;
    g_jog_note.clear();
    refresh_jog();
}

// What the operator asked, once the gate lets it go: the job of the screen is started, held, let go on or answered at a tool change.
static void run_action(const hmi::Action& go) {
    if (go.kind == hmi::Kind::Jog) {
        run_jog(go);
        return;
    }
    ServerJob job;
    if (go.op == hmi::Op::StartJob) job.what = ServerJob::Do::Start;
    else if (go.op == hmi::Op::Pause) job.what = ServerJob::Do::Pause;
    else if (go.op == hmi::Op::ResumeJob) job.what = ServerJob::Do::Release;
    else if (go.op == hmi::Op::ToolChanged) job.what = ServerJob::Do::Confirm;
    else {
        note(hmi::ScreenNote::MoveNoLink);
        return;
    }
    if (xQueueSend(g_to_server, &job, 0) != pdTRUE) note(hmi::ScreenNote::Busy);
}

// The task of the link: asks for the fleet every `poll_s` seconds and does what the operator asked, one request at a time.
static void link_task(void*) {
    TickType_t next_poll = 0;
    unsigned polls = 0;
    for (;;) {
        ServerJob job;
        const bool asked = xQueueReceive(g_to_server, &job, pdMS_TO_TICKS(200)) == pdTRUE;
        hmi::CallError error = hmi::CallError::None;
        hmi::Response r;
        if (asked && job.what >= ServerJob::Do::Light) {
            quality_job(job);
        } else if (asked) {
            hmi::Request req;
            xSemaphoreTake(g_lock, portMAX_DELAY);
            switch (job.what) {
                case ServerJob::Do::Create: req = g_flow.create_request(g_settings.server_machine); break;
                case ServerJob::Do::Start: req = g_flow.start_request(); break;
                case ServerJob::Do::Confirm: req = g_flow.confirm_request(); break;
                case ServerJob::Do::ResumeFrom: req = g_flow.resume_request(); break;
                case ServerJob::Do::Pause: req = g_flow.pause_request(); break;
                case ServerJob::Do::Release: req = g_flow.release_request(); break;
                // the quality check has its own function (quality_job): it never gets here
                case ServerJob::Do::Light:
                case ServerJob::Do::Inspect:
                case ServerJob::Do::Preset:
                case ServerJob::Do::Calibrate:
                case ServerJob::Do::Last: break;
            }
            xSemaphoreGive(g_lock);
            if (req.path.empty()) {
                note(hmi::ScreenNote::NothingToSend);
            } else if (!srvlink::call(g_settings, req, r, error)) {
                if (job.what == ServerJob::Do::ResumeFrom) {
                    xSemaphoreTake(g_lock, portMAX_DELAY);
                    g_flow.resume_failed();
                    xSemaphoreGive(g_lock);
                }
                set_note(said(error));
            } else if (job.what == ServerJob::Do::ResumeFrom) {
                // the server makes the new job from the request it kept; an old one has no such route and the screen sends the file it keeps
                std::string why;
                xSemaphoreTake(g_lock, portMAX_DELAY);
                const hmi::ResumeStep step = g_flow.on_resume_from(r, why);
                xSemaphoreGive(g_lock);
                if (step == hmi::ResumeStep::Fallback) {
                    ServerJob again;
                    again.what = ServerJob::Do::Create;
                    xQueueSend(g_to_server, &again, 0);
                    note(hmi::ScreenNote::OldServer);
                } else {
                    if (step == hmi::ResumeStep::Made) note(hmi::ScreenNote::GoingOn); else set_note(hmi::reason_text(why, g_settings.language));
                }
            } else {
                std::string why;
                xSemaphoreTake(g_lock, portMAX_DELAY);
                bool ok = true;
                bool retry_plain = false;
                if (job.what == ServerJob::Do::Create) {
                    ok = g_flow.on_created(r, why);
                    retry_plain = !ok && g_flow.take_rack_retry();  // the server turned the bar of the tool changer down: the job is sent again without it
                }
                else if (job.what == ServerJob::Do::Start) ok = g_flow.on_started(r, why);
                else if (r.status != 200 && r.status != 202) { ok = false; why = hmi::error_line(r); }
                const std::string base = g_flow.report_base();
                const std::string name = g_flow.name();
                const std::string job_id = g_flow.job_id();
                xSemaphoreGive(g_lock);
                if (ok && job.what == ServerJob::Do::Start) memory_save({job_id, g_settings.server_machine, name});
                if (retry_plain) {
                    ServerJob again;
                    again.what = ServerJob::Do::Create;
                    xQueueSend(g_to_server, &again, 0);
                    set_note(hmi::text(hmi::T::RackNotChained, g_settings.language));
                } else if (ok) {
                    note(hmi::ScreenNote::Ok);
                } else {
                    set_note(hmi::reason_text(why, g_settings.language));
                }
                if (ok && job.what == ServerJob::Do::Create) {
                    // the answer of the server is kept on the card, without replacing an older report
                    const std::string file = hmi::free_name(base, "txt", [](const std::string& n) {
                        std::string tmp;
                        return board::sd_read(n, tmp, 1);
                    });
                    board::sd_write(file, "job " + name + "\n" + r.body + "\n");
                }
            }
        }
        if (xTaskGetTickCount() >= next_poll) {
            next_poll = xTaskGetTickCount() + pdMS_TO_TICKS(g_settings.poll_s * 1000);
            hmi::Response fr;
            hmi::FleetView view;
            // After a start or a loss of the link the first request is the health check (open to anyone, no token goes with it): it tells "nobody answers" from "something answers that is not
            // an EH3D server" before the fleet is asked for. A server that answers it (well or degraded) goes on to the fleet as before.
            hmi::Health health = hmi::Health::Ok;
            if (g_link_said != hmi::Link::Connected) health = hmi::classify_health(srvlink::call(g_settings, hmi::health_request(), fr, error), fr);
            const bool reached = health == hmi::Health::Silent ? false : health == hmi::Health::NotEh3d ? true : srvlink::call(g_settings, hmi::fleet_request(), fr, error);
            const bool parsed = reached && health != hmi::Health::NotEh3d && fr.status == 200 && hmi::parse_fleet(fr.body, view);
            link_update(hmi::classify_fleet(reached, fr, parsed), !parsed || hmi::fleet_has_machine(view, g_settings.server_machine), fr);
            if (parsed) {
                xSemaphoreTake(g_lock, portMAX_DELAY);
                g_fleet = view;
                g_fleet_new = true;
                g_server_ok = true;
                g_flow.on_fleet(view, g_settings.server_machine);
                const hmi::Request ended = g_flow.report_request();
                xSemaphoreGive(g_lock);
                // a job that left the machine: the server says if it was done or stopped (a stopped one can go on from the holes it did)
                if (!ended.path.empty()) {
                    hmi::Response rr;
                    const bool got = srvlink::call(g_settings, ended, rr, error);
                    xSemaphoreTake(g_lock, portMAX_DELAY);
                    if (got) g_flow.on_report(rr);
                    // the drilling is done and the routing comes after it (or the other way round for the files chosen): its job is made and waits for the operator
                    const bool next = got && g_flow.advance();
                    const std::string next_name = g_flow.name();
                    xSemaphoreGive(g_lock);
                    if (next) {
                        ServerJob again;
                        again.what = ServerJob::Do::Create;
                        xQueueSend(g_to_server, &again, 0);
                        set_note(std::string(hmi::text(hmi::T::Next, g_settings.language)) + ": " + next_name);
                    }
                }
                // the spindle of the machine: asked at the first reading and then every tenth
                if (!g_settings.server_machine.empty() && (polls++ % 10) == 0) {
                    hmi::Response sr;
                    hmi::SpindleView sv;
                    if (srvlink::call(g_settings, hmi::spindle_request(g_settings.server_machine), sr, error) && sr.status == 200 && hmi::parse_spindle(sr.body, sv)) {
                        xSemaphoreTake(g_lock, portMAX_DELAY);
                        g_spindle = sv;
                        g_spindle_new = true;
                        xSemaphoreGive(g_lock);
                    }
                    // the table of the machine, for the warning of a board that does not fit
                    hmi::Response mr;
                    hmi::TableSize ts;
                    if (srvlink::call(g_settings, hmi::machine_request(g_settings.server_machine), mr, error) && mr.status == 200 && hmi::parse_machine_table(mr.body, ts)) {
                        xSemaphoreTake(g_lock, portMAX_DELAY);
                        g_table = ts;
                        g_spindle_new = true;
                        xSemaphoreGive(g_lock);
                    }
                }
            } else {
                xSemaphoreTake(g_lock, portMAX_DELAY);
                g_server_ok = false;
                xSemaphoreGive(g_lock);
            }
        }
    }
}

// Shows the lines of the machine page about the work in hand (the LVGL lock is taken by the caller).
static void refresh_hud() {
    xSemaphoreTake(g_lock, portMAX_DELAY);
    std::string text;
    if (g_settings.tool_bar) text = hmi::hud_text(g_flow.bar(), g_settings.language, g_flow.model(), g_flow.route(), g_flow.tool_wait(), g_flow.pause_tool());
    // the card of the work in hand: the machine as the fleet reports it, and the wall clock (the system's when it is set, else the server's)
    const hmi::JobBar bar = g_flow.bar();
    const hmi::MachineRow* row = nullptr;
    for (const hmi::MachineRow& m : g_fleet.machines)
        if (m.id == g_settings.server_machine) row = &m;
    long now = static_cast<long>(std::time(nullptr));
    if (now < hmi::kClockSetAfter) now = g_fleet.server_time_s;
    const int current_tool = bar.current >= 0 ? bar.tools[static_cast<std::size_t>(bar.current)].number : 0;
    const hmi::JobCard card = hmi::make_job_card(bar, row, g_flow.route(), now, g_settings.clock_utc_offset_min, current_tool > 0 ? g_flow.rack_slot(g_flow.route(), current_tool) : 0);
    // the bar of the tool changer
    ui::RackView rack;
    rack.atc = g_flow.atc();
    rack.tools = g_flow.rack_bar();
    rack.places = g_flow.rack_places();
    rack.problems = hmi::rack_problem_text(g_flow.rack_problems(), g_settings.language);
    if (g_flow.rack_reply() == hmi::Workflow::RackReply::NotApplied) rack.not_chained = hmi::text(hmi::T::RackNotChained, g_settings.language);
    xSemaphoreGive(g_lock);
    ui::show_hud(text);
    ui::show_job_card(hmi::card_text(card, g_settings.language));
    ui::show_rack(rack, g_settings);
    refresh_jog();
}

// Shows the tools of the chosen file and the spindle of the machine (the LVGL lock is taken by the caller).
static void refresh_job() {
    xSemaphoreTake(g_lock, portMAX_DELAY);
    const hmi::ToolSwitch tools = g_flow.tool_switch();
    const hmi::SpindleView spindle = g_spindle;
    ui::FlowState flow;
    flow.stopped = g_flow.stage() == hmi::Stage::Stopped;
    flow.failed = g_flow.outcome() == hmi::Outcome::Failed;
    // the table: the server's, or the one of the settings when the server does not say it
    const hmi::TableSize table = g_table.known() ? g_table : hmi::TableSize{static_cast<double>(g_settings.table_width_mm), static_cast<double>(g_settings.table_depth_mm)};
    const hmi::JobAdvice advice = g_flow.advice(table);
    flow.can_resume = g_flow.can_resume();
    flow.route = g_flow.route();
    flow.done = g_flow.done();
    flow.total = g_flow.total();
    flow.skipped = g_flow.skipped();
    flow.next = g_flow.next_name();
    flow.message = g_flow.message();
    flow.resumed_from = g_flow.resumed_from();
    flow.held = g_flow.held();
    flow.tool_wait = g_flow.tool_wait();
    xSemaphoreGive(g_lock);
    ui::show_job(tools, spindle, advice, g_settings);
    ui::show_flow(flow, g_settings);
    refresh_hud();
}

// The stack each task has left, as the lowest it has ever been (bytes): logged now and then, and a warning when one is short, so an overflow shows on the bench before it corrupts memory.
static void stack_report() {
    struct Entry {
        const char* name;
        TaskHandle_t handle;
    };
    const Entry tasks[] = {{"main", xTaskGetCurrentTaskHandle()}, {"say", g_say_task}, {"link", g_link_task}, {"stop", g_stop_task}};
    for (const Entry& t : tasks) {
        if (t.handle == nullptr) continue;
        const unsigned left = static_cast<unsigned>(uxTaskGetStackHighWaterMark(t.handle));
        if (left < CONFIG_HMI_STACK_WARN_BYTES) ESP_LOGW(TAG, "task %s has only %u bytes of stack left", t.name, left);
        else ESP_LOGI(TAG, "task %s: %u bytes of stack left", t.name, left);
    }
}

static void rebuild(bool mounted_portrait) {
    const hmi::Layout layout = hmi::layout_for(g_settings.orientation, mounted_portrait);
    board::lvgl_lock();
    board::set_rotation(layout.rotation);
    ui::build(g_settings, layout, post);
    ui::show_config_issue(config_issue());
    ui::show_card(board::sd_list(), g_settings);
    refresh_job();
    board::lvgl_unlock();
}

extern "C" void app_main() {
    {
        const esp_err_t e = nvs_flash_init();
        if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            nvs_flash_erase();
            nvs_flash_init();
        }
    }
    esp_vfs_spiffs_conf_t fs = {"/config", "config", 4, false};
    if (esp_vfs_spiffs_register(&fs) != ESP_OK) ESP_LOGW(TAG, "no config partition: the defaults are used");
    g_cfg_missing = [] {
        FILE* f = std::fopen(CONFIG_HMI_CONFIG_PATH, "rb");
        if (f == nullptr) return true;
        std::fclose(f);
        return false;
    }();
    g_settings = hmi::parse_settings(read_file(CONFIG_HMI_CONFIG_PATH), g_problems);
    for (const auto& p : g_problems) ESP_LOGW(TAG, "settings line %d (%s): %s", p.line, p.key.c_str(), p.what.c_str());
    {
        // who this firmware is: shown on the screen and sent to the server in every request
        const esp_app_desc_t* app = esp_app_get_description();
        g_version = app->version;
        srvlink::set_client(hmi::client_id(app->project_name, app->version));
        ui::set_version(g_version);
    }

    if (!board::display_start()) {
        ESP_LOGE(TAG, "the display does not start");
        imgconfirm::display_failed();
        return;
    }
    imgconfirm::start();   // an image on probation is confirmed after a while of running
    board::set_brightness(g_settings.brightness_percent);
    if (!board::audio_start()) ESP_LOGW(TAG, "no sound: the alerts are only shown");
    if (!board::sd_mount()) ESP_LOGW(TAG, "no microSD card: files cannot be opened or saved");

    g_requests = xQueueCreate(8, sizeof(ui::Request));
    g_to_server = xQueueCreate(4, sizeof(ServerJob));
    g_say = xQueueCreate(4, sizeof(hmi::Cue));
    xTaskCreate(say_task, "say", 8192, nullptr, 3, &g_say_task);
    g_lock = xSemaphoreCreateMutex();
    // a job that was running when the screen went down: follow it again; the operator can leave it with the cancel button
    {
        hmi::JobMemory m;
        if (memory_load(m) && m.machine == g_settings.server_machine && g_flow.adopt(m.job_id, m.name)) {
            g_remembered = true;
            note(hmi::ScreenNote::Following, m.name);
        } else {
            memory_clear();  // what was kept is not for this machine, or it is damaged
        }
    }
    xTaskCreate(link_task, "link", 8192, nullptr, 4, &g_link_task);
    xTaskCreate(stop_task, "stop", 6144, nullptr, 6, &g_stop_task);  // above the link and the screen: a stop is not queued behind anything
    rebuild(false);
    {
        // the last check of the machine, so the screen shows what the desktop program found too
        ServerJob last;
        last.what = ServerJob::Do::Last;
        xQueueSend(g_to_server, &last, 0);
    }

    g_flow.configure_rack(g_settings);
    g_jog = new hmi::JogPanel(g_settings, hmi::make_jog_limits(static_cast<double>(g_settings.table_width_mm), static_cast<double>(g_settings.table_depth_mm), g_settings));
    board::lvgl_lock();
    refresh_job();  // the panel and the bar show their first state
    board::lvgl_unlock();
    hmi::Gate gate(g_settings);
    ui::State state;
    hmi::Stage last_stage = hmi::Stage::Nothing;
    bool last_ended_well = false;
    int last_stages = 0;
    bool fleet_stale_shown = false;
    std::uint32_t next_stack_report = board::now_ms() + 5000;
    for (;;) {
        bool wired = false, pressed = false;
        board::read_hardware_stop(wired, pressed);
        gate.hardware_stop(wired, pressed);
        if (gate.take_stop_edge()) request_stop();  // the stop input pressed: the job of the machine is stopped on the server too
        gate.link(state.armed);  // `armed` follows the job of the server; until a job runs the gate refuses every move
        gate.tick(board::now_ms());
        {
            // the top speed of the spindle the machine carries (the server says it): an order for more turns is cut to it or refused
            xSemaphoreTake(g_lock, portMAX_DELAY);
            gate.machine_spindle_max(g_spindle.known ? g_spindle.max_rpm : 0);
            xSemaphoreGive(g_lock);
        }

        g_estop_now = gate.latched() || pressed;
        if (g_jog != nullptr && g_jog->held()) {
            // the stretches of a held arrow, worked out on a copy that is kept only if they went; a stop, a job or the end of the travel ends the hold
            hmi::JogPanel trial = *g_jog;
            std::vector<hmi::JogOrder> stretches;
            const bool running = state.armed;
            const hmi::JogRefusal ended = trial.tick(0.05, hmi::JogGate{running, g_estop_now}, stretches);
            if (ended != hmi::JogRefusal::None) {
                g_jog->release();
                g_jog_note = jog_refusal_text(ended);
            } else if (!stretches.empty() && send_jog_orders(stretches)) {
                *g_jog = trial;
            } else if (!stretches.empty()) {
                g_jog->release();
            }
        }
        ui::Request r;
        while (xQueueReceive(g_requests, &r, 0) == pdTRUE) {
            board::lvgl_lock();
            // a press on the panel, the home button and a place to go to are orders like any other: they go through the gate
            if (g_jog != nullptr) {
                if (r.what == ui::What::JogPress && r.index >= 0 && r.index < 6) {
                    g_finger_down = true;
                    r.action = {hmi::Kind::Jog, hmi::jog_order_id(kJogDirs[r.index], g_jog->step_mm(), g_jog->continuous())};
                    r.what = ui::What::Action;
                } else if (r.what == ui::What::JogHome) {
                    r.action = {hmi::Kind::Jog, hmi::jog_home_id()};
                    r.what = ui::What::Action;
                } else if (r.what == ui::What::JogPlace && r.index >= 0 && r.index < hmi::JogPanel::kPlaces) {
                    if (r.save) {
                        g_jog->save_place(static_cast<std::size_t>(r.index));  // keeping a place moves nothing
                        refresh_jog();
                    } else if (g_jog->has_place(static_cast<std::size_t>(r.index))) {
                        r.action = {hmi::Kind::Jog, hmi::jog_goto_id(r.index)};
                        r.what = ui::What::Action;
                    }
                }
            }
            switch (r.what) {
                case ui::What::JogPress:
                case ui::What::JogHome:
                case ui::What::JogPlace:
                    break;
                case ui::What::JogRelease:
                    g_finger_down = false;
                    if (g_jog != nullptr) g_jog->release();
                    break;
                case ui::What::JogStep:
                    if (g_jog != nullptr) g_jog->set_step(static_cast<std::size_t>(r.index));
                    refresh_jog();
                    break;
                case ui::What::JogMode:
                    if (g_jog != nullptr) g_jog->set_continuous(!g_jog->continuous());
                    refresh_jog();
                    break;
                case ui::What::JogSpeed:
                    if (g_jog != nullptr) g_jog->set_speed(r.index == 0 ? hmi::JogSpeed::Slow : (r.index == 1 ? hmi::JogSpeed::Medium : hmi::JogSpeed::Fast));
                    refresh_jog();
                    break;
                case ui::What::RackSet: {
                    xSemaphoreTake(g_lock, portMAX_DELAY);
                    g_flow.set_rack_place(r.route_tool, r.tool, r.slot);
                    xSemaphoreGive(g_lock);
                    refresh_job();
                    break;
                }
                case ui::What::AckRelease:
                    gate.acknowledge_release();
                    break;
                case ui::What::Cancel:
                    gate.cancel();
                    ui::ask(nullptr, g_settings);
                    break;
                case ui::What::Confirm: {
                    hmi::Action go;
                    if (gate.confirm(hmi::Origin::Touch, board::now_ms(), go)) {
                        ESP_LOGI(TAG, "confirmed: %s", go.what.c_str());
                        run_action(go);
                    }
                    ui::ask(nullptr, g_settings);
                    break;
                }
                case ui::What::Action:
                    if (r.action.kind == hmi::Kind::Stop) request_stop();  // a stop always goes, by the task of its own
                    {
                        // a start with no job ready is answered here, before the gate asks anything
                        if (r.action.kind == hmi::Kind::Start) {
                            xSemaphoreTake(g_lock, portMAX_DELAY);
                            const bool ready = !g_flow.start_request().path.empty();
                            xSemaphoreGive(g_lock);
                            if (!ready) {
                                note(hmi::ScreenNote::NoJobReady);
                                break;
                            }
                            // on a machine with the tool changer, a tool with no place, a place the machine does not have or two tools in one place stop the start and say so
                            xSemaphoreTake(g_lock, portMAX_DELAY);
                            const std::string bad = hmi::rack_problem_text(g_flow.rack_problems(), g_settings.language);
                            xSemaphoreGive(g_lock);
                            if (!bad.empty()) {
                                set_note(bad);
                                refresh_job();
                                break;
                            }
                        }
                        hmi::Decision d = gate.request(r.action, hmi::Origin::Touch, board::now_ms());
                        if (d.verdict == hmi::Verdict::NeedConfirm) {
                            // the confirmation shows the turns the spindle will really get; when they were cut the screen says so
                            // the start of a job also shows the plan the server made: the estimated time and what looks wrong in the work
                            std::string plan;
                            if (r.action.kind == hmi::Kind::Start) {
                                xSemaphoreTake(g_lock, portMAX_DELAY);
                                const hmi::TableSize table = g_table.known() ? g_table : hmi::TableSize{static_cast<double>(g_settings.table_width_mm), static_cast<double>(g_settings.table_depth_mm)};
                                plan = hmi::plan_text(g_flow.advice(table).plan, g_settings.language);
                                xSemaphoreGive(g_lock);
                            }
                            ui::ask(&gate.pending_action(), g_settings, gate.remaining_ms(board::now_ms()), plan);
                            if (d.clipped) set_note(std::string(hmi::text(hmi::T::SpindleCut, g_settings.language)) + ": " + std::to_string(d.asked_rpm) + " > " + std::to_string(d.rpm) + " rpm");
                        } else if (d.verdict == hmi::Verdict::Refused) {
                            ui::refused(d.why, g_settings);
                        } else {
                            // the bench setting without the question: the order goes now (a stop and a question have nothing more to do)
                            if (d.clipped) set_note(std::string(hmi::text(hmi::T::SpindleCut, g_settings.language)) + ": " + std::to_string(d.asked_rpm) + " > " + std::to_string(d.rpm) + " rpm");
                            if (r.action.kind != hmi::Kind::Stop && r.action.kind != hmi::Kind::Info) run_action(r.action);
                        }
                    }
                    break;
                case ui::What::SetSpindleOver:
                    g_settings.spindle_over = r.refuse ? hmi::SpindleOver::Refuse : hmi::SpindleOver::Clip;  // kept until the next start: hmi.cfg (spindle_over = ...) holds the lasting choice
                    break;
                case ui::What::Resume: {
                    std::string why;
                    xSemaphoreTake(g_lock, portMAX_DELAY);
                    // the server makes the new job from the request it kept (it works after a reset, with no file); an old server is answered with the file the screen keeps
                    const bool by_server = g_flow.can_resume() && g_flow.resumes_by_server();
                    const bool ok = by_server || g_flow.resume(why);
                    xSemaphoreGive(g_lock);
                    if (ok) {
                        ServerJob job;
                        job.what = by_server ? ServerJob::Do::ResumeFrom : ServerJob::Do::Create;
                        xQueueSend(g_to_server, &job, 0);
                        note(hmi::ScreenNote::GoingOn);
                        refresh_job();
                    } else {
                        set_note(hmi::reason_text(why, g_settings.language));
                    }
                    break;
                }
                case ui::What::SetOrientation:
                    g_settings.orientation = r.orientation;  // kept until the next start: hmi.cfg holds the lasting choice
                    board::lvgl_unlock();
                    rebuild(false);
                    board::lvgl_lock();
                    break;
                case ui::What::SetLanguage:
                    g_settings.language = r.language;  // kept until the next start: hmi.cfg (language = ...) holds the lasting choice
                    board::lvgl_unlock();
                    rebuild(false);
                    board::lvgl_lock();
                    break;
                case ui::What::RefreshCard:
                    ui::show_card(board::sd_list(), g_settings);
                    break;
                case ui::What::OpenCard: {
                    std::string text, why;
                    xSemaphoreTake(g_lock, portMAX_DELAY);
                    bool ok = board::sd_read(r.file, text, 512 * 1024) && g_flow.choose(r.file, text, why);
                    if (ok) {
                        // the tool table of the same name, when the card has one
                        std::string base = r.file;
                        base.resize(base.rfind('.'));
                        std::string tools;
                        board::sd_read(base + (hmi::kind_of_file(r.file) == hmi::FileKind::Route ? ".rtd" : ".dtd"), tools, 64 * 1024);
                        g_flow.set_tools(tools);
                    }
                    xSemaphoreGive(g_lock);
                    if (ok) note(hmi::ScreenNote::File, r.file); else if (why.empty()) note(hmi::ScreenNote::Unreadable); else set_note(hmi::reason_text(why, g_settings.language));
                    if (ok) refresh_job();
                    break;
                }
                case ui::What::ToggleTool: {
                    xSemaphoreTake(g_lock, portMAX_DELAY);
                    g_flow.toggle_tool(r.tool);
                    xSemaphoreGive(g_lock);
                    refresh_job();
                    break;
                }
                case ui::What::LightSet:
                case ui::What::InspectRun:
                case ui::What::InspectPreset:
                case ui::What::InspectCalibrate: {
                    ServerJob job;
                    xSemaphoreTake(g_lock, portMAX_DELAY);
                    if (r.what == ui::What::LightSet) { g_arg_light = r.light; job.what = ServerJob::Do::Light; }
                    else if (r.what == ui::What::InspectRun) { g_arg_light = r.light; g_arg_hole = r.hole; g_quality_running = true; job.what = ServerJob::Do::Inspect; }
                    else if (r.what == ui::What::InspectPreset) { g_arg_preset = r.preset; job.what = ServerJob::Do::Preset; }
                    else job.what = ServerJob::Do::Calibrate;
                    xSemaphoreGive(g_lock);
                    xQueueSend(g_to_server, &job, 0);
                    break;
                }
                case ui::What::SendJob: {
                    xSemaphoreTake(g_lock, portMAX_DELAY);
                    // a drilling file with every tool off while a routing file waits: the routing is the job
                    const bool moved_on = g_flow.nothing_to_do() && g_flow.skip_empty();
                    const std::string moved_to = g_flow.name();
                    const bool nothing = g_flow.nothing_to_do();
                    xSemaphoreGive(g_lock);
                    if (moved_on) {
                        set_note(std::string(hmi::text(hmi::T::Next, g_settings.language)) + ": " + moved_to);
                        refresh_job();
                    }
                    if (nothing) {
                        note(hmi::ScreenNote::AllOff);
                        break;
                    }
                    ServerJob job;
                    job.what = ServerJob::Do::Create;
                    xQueueSend(g_to_server, &job, 0);
                    break;
                }
            }
            board::lvgl_unlock();
        }

        // what the link task found out: the fleet, the note and the stage of the job
        xSemaphoreTake(g_lock, portMAX_DELAY);
        const bool fleet_new = g_fleet_new;
        const bool spindle_new = g_spindle_new;
        g_spindle_new = false;
        const hmi::FleetView fleet = g_fleet;
        g_fleet_new = false;
        const hmi::QualityView quality = g_quality;
        const hmi::LightView light = g_light;
        const bool quality_new = g_quality_new || g_quality_running;
        const bool quality_running = g_quality_running;
        g_quality_new = false;
        state.server_ok = g_server_ok;
        {
            // the link and the age of the data: an old value is never shown as a current one
            const std::uint32_t now = board::now_ms();
            state.link = g_link.state();
            state.stale = g_link.stale(now, g_settings.stale_after_s);
            state.age_s = g_link.age_s(now);
        }
        state.note = g_note;
        state.percent = g_flow.percent();
        // the machine counts as armed while the job of this screen runs or waits for the operator
        state.armed = g_flow.stage() == hmi::Stage::Running || g_flow.stage() == hmi::Stage::Waiting;
        // over for good: finished well, or stopped or failed (kept to go on from). A job that left the machine and that the server has not been asked about is neither
        const bool over = g_flow.settled();
        const bool ended_well = g_flow.ended_well();
        const hmi::Stage stage_now = g_flow.stage();
        const int stages_done = g_flow.stages_done();
        const bool job_complete = g_flow.job_complete();
        const std::string stage_next = g_flow.stage_next_name();
        xSemaphoreGive(g_lock);
        if (over && g_remembered) memory_clear();
        // the notices of the job, once each time the stage changes into them
        bool stage_changed = false;
        if (stage_now != last_stage) {
            if (stage_now == hmi::Stage::Waiting) say(hmi::Cue::ToolChange);
            last_stage = stage_now;
            stage_changed = true;
        }
        // the end of a program is not always the end of the job: a drilling that ends with the routing still to come is a stage of its own (its notice and its window), and the
        // sound of the end and the order to take the board out wait until the whole job is done, and only when the server said it did all its work
        std::string stage_window;
        switch (g_end.update(stages_done, job_complete)) {
            case hmi::EndNotice::StageDone:
                say(hmi::Cue::Stage);
                stage_window = hmi::stage_text(stage_next, g_settings.language);
                break;
            case hmi::EndNotice::BoardOut:
                say(hmi::Cue::Finished);
                set_note(hmi::text(hmi::T::BoardOut, g_settings.language));
                break;
            case hmi::EndNotice::None: break;
        }
        if (ended_well != last_ended_well) {
            last_ended_well = ended_well;
            stage_changed = true;
        }
        if (stages_done != last_stages) {
            last_stages = stages_done;
            stage_changed = true;
        }
        state.stop_wired = wired;
        state.stop_pressed = pressed;
        state.stop_latched = gate.latched();
        board::lvgl_lock();
        if (!stage_window.empty()) ui::show_stage(stage_window, g_settings);
        ui::show(state, g_settings);
        if (fleet_new || state.stale != fleet_stale_shown) ui::show_fleet(fleet, g_settings, state.stale);
        fleet_stale_shown = state.stale;
        if (spindle_new || stage_changed) refresh_job();
        else if (fleet_new) refresh_hud();  // the progress and the model change with every reading; the tool chips are not rebuilt for them
        if (quality_new) ui::show_quality(quality, light, quality_running, g_settings);
        if (gate.pending()) ui::ask_tick(gate.remaining_ms(board::now_ms()), g_settings);  // the countdown to the abort
        else ui::ask(nullptr, g_settings);
        board::lvgl_unlock();
        if (CONFIG_HMI_STACK_REPORT_S > 0 && static_cast<std::int32_t>(board::now_ms() - next_stack_report) >= 0) {
            stack_report();
            next_stack_report = board::now_ms() + CONFIG_HMI_STACK_REPORT_S * 1000u;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
