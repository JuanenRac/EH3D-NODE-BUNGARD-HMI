# Changelog

All notable changes to this project are documented here.

## [Unreleased]

- **The job is followed again after a reset:** `core/job_memory.hpp` keeps the id, the machine and the name of the job that the operator started (in the flash, until it is over), and `Workflow::adopt` follows it again after a power cut or a reset; the next reading of the server says whether it runs, waits or is over. 10 more checks.

- **The right keyboard for each setting:** a numeric pad that knows the range and the decimals of the setting, and a full keyboard with the keys of each of the eight languages (a one-letter shift, a layer of symbols, the length of the setting); every setting that the screen edits is described once.
- **Buttons with a symbol:** the jog pad, home, the spindle, pause, start, stop and the answers of a question use symbols and the letter of the axis, in the colours of the desktop program.
- **Recorded notices:** `sound_finish` and `sound_tool_change` (`off`, `default` or `voice:LANG:f|m`) and the file that each one names in the folder `voices`; the speaker is not wired yet.
## [0.0.1] - The rules of the screen, the settings file, the spoken orders and the first screens

- **The gate:** every move and every spindle order, from a touch or from the voice, passes one place that runs it, asks for a confirmation on the screen with a time limit or refuses it with a reason; a stop always goes, the hardware stop is a latch that only an acknowledgement on the screen clears once the button is up, nothing moves while the link is not armed, and a spoken order always asks.
- **The settings file:** `hmi.cfg` holds every tuned value (language, brightness, the server, the safety switches, the voice); a value out of range or an unknown key is reported and the safe default stays. `hmi.cfg.example` lists every key.
- **Voice:** spoken alerts and a short fixed list of spoken orders in eight languages; the orders that only halt are understood inside a sentence, the ones that move or spin must be said whole.
- **Eight languages:** English, Spanish, Italian, German, French, European Portuguese, Chinese and Japanese in the screens, the alerts and the orders.
- **Firmware:** the ESP-IDF project for the ESP32-P4 module with the LVGL machine screen, the stop button, the confirmation window and the hardware-stop input (written; not built or run on a module yet).
- **Standing up or lying down:** the setting `orientation` (`auto`, `landscape`, `portrait`) turns the picture of the panel; lying down the job, the plan view and the jog pad sit in three columns, standing up they stack, and a touch lands where the screen is touched.
- **microSD card:** the drilling, routing, Gerber, DXF and ZIP files of the card are listed, a file is chosen and read, and reports are written; only plain file names are accepted.
- **One more client of the server:** the screen reads the fleet every `poll_s` seconds, sends the chosen file as a job that waits ready, starts it through the gate, follows its progress and its stops for a tool change in real time, and keeps the answer of the server on the card.
- **The look of the desktop program:** the Forge theme, cards with cut corners, chips in the top bar, the bar of the position with the round stop button, and a preview of the screens in the README drawn from the texts of the firmware.
- **Tests:** two host test programs with 743 checks.
