// EH3D-NODE-BUNGARD-HMI - main/app_main.cpp: reads the settings, starts the module, and runs every request of the operator, from a
// touch or from the voice, through the gate (core/gate.hpp). This has not been built or run on a module yet. GPL-3.0-or-later.
#include <cstdio>
#include <string>
#include <vector>

#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "../core/hmi_config.hpp"
#include "../core/ui_text.hpp"
#include "../core/voice.hpp"
#include "board.hpp"
#include "ui.hpp"

static const char* TAG = "hmi";
static hmi::Settings g_settings;
static QueueHandle_t g_requests = nullptr;

// called by the screens in the LVGL task: the request is only queued, the gate is run by the application task
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

extern "C" void app_main() {
    esp_vfs_spiffs_conf_t fs = {"/config", "config", 4, false};
    if (esp_vfs_spiffs_register(&fs) != ESP_OK) ESP_LOGW(TAG, "no config partition: the defaults are used");
    std::vector<hmi::Problem> problems;
    g_settings = hmi::parse_settings(read_file(CONFIG_HMI_CONFIG_PATH), problems);
    for (const auto& p : problems) ESP_LOGW(TAG, "settings line %d (%s): %s", p.line, p.key.c_str(), p.what.c_str());

    if (!board::display_start()) {
        ESP_LOGE(TAG, "the display does not start");
        return;
    }
    board::set_brightness(g_settings.brightness_percent);
    if (!board::audio_start()) ESP_LOGW(TAG, "no sound: the alerts are only shown");

    g_requests = xQueueCreate(8, sizeof(ui::Request));
    board::lvgl_lock();
    ui::build(g_settings, post);
    board::lvgl_unlock();

    hmi::Gate gate(g_settings);
    ui::State state;
    for (;;) {
        bool wired = false, pressed = false;
        board::read_hardware_stop(wired, pressed);
        gate.hardware_stop(wired, pressed);
        gate.link(state.armed);  // the link to the machine sets `armed`; until it exists the gate refuses every move
        gate.tick(board::now_ms());

        ui::Request r;
        while (xQueueReceive(g_requests, &r, 0) == pdTRUE) {
            board::lvgl_lock();
            if (r.release_acknowledged) {
                gate.acknowledge_release();
            } else if (r.cancelled) {
                gate.cancel();
                ui::ask(nullptr, g_settings);
            } else if (r.confirmed) {
                hmi::Action go;
                if (gate.confirm(hmi::Origin::Touch, board::now_ms(), go)) ESP_LOGI(TAG, "confirmed: %s", go.what.c_str());
                ui::ask(nullptr, g_settings);
            } else {
                hmi::Decision d = gate.request(r.action, hmi::Origin::Touch, board::now_ms());
                if (d.verdict == hmi::Verdict::NeedConfirm) {
                    ui::ask(&r.action, g_settings);
                } else if (d.verdict == hmi::Verdict::Refused) {
                    ui::refused(d.why, g_settings);
                }
            }
            board::lvgl_unlock();
        }
        state.stop_wired = wired;
        state.stop_pressed = pressed;
        state.stop_latched = gate.latched();
        board::lvgl_lock();
        ui::show(state, g_settings);
        if (!gate.pending()) ui::ask(nullptr, g_settings);
        board::lvgl_unlock();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
