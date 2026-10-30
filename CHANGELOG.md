# Changelog

All notable changes to this project are documented here.

## [Unreleased]

## [0.0.1] - The rules of the screen, the settings file, the spoken orders and the first screens

- **The gate:** every move and every spindle order, from a touch or from the voice, passes one place that runs it, asks for a confirmation on the screen with a time limit or refuses it with a reason; a stop always goes, the hardware stop is a latch that only an acknowledgement on the screen clears once the button is up, nothing moves while the link is not armed, and a spoken order always asks.
- **The settings file:** `hmi.cfg` holds every tuned value (language, brightness, the server, the safety switches, the voice); a value out of range or an unknown key is reported and the safe default stays. `hmi.cfg.example` lists every key.
- **Voice:** spoken alerts and a short fixed list of spoken orders in eight languages; the orders that only halt are understood inside a sentence, the ones that move or spin must be said whole.
- **Eight languages:** English, Spanish, Italian, German, French, European Portuguese, Chinese and Japanese in the screens, the alerts and the orders.
- **Firmware:** the ESP-IDF project for the ESP32-P4 module with the LVGL machine screen, the stop button, the confirmation window and the hardware-stop input (written; not built or run on a module yet).
- **Tests:** one host test program with 598 checks.
