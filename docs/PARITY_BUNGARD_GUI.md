# What the screen shares with the desktop program

EH3D-BUNGARD-GUI is the program on the computer that drives the same machine; this screen is the control of the machine on its own. This page lists, one by one, what the desktop
program does and where the screen stands on it. The states:

* **DONE** - written in the screen and tested on a computer (`tests/test_parity.cpp` and the other host tests). Nothing of it has run on a module.
* **MISSING** - applies to a screen of 10.1 inches and is not written yet; the reason is given.
* **N/A** - it does not apply to this screen, and why.
* **DIFFERS** - it is done, but in another way because of the system or the size of the screen.

Every tuned value of the new parts is a line of `hmi.cfg` (see `hmi.cfg.example`), never a constant of the code, and each safety rule has a switch there that starts in its
conservative position.

| Function of the desktop program | State on the screen | Notes |
|---|---|---|
| Card under the progress bar with four blocks: the tool and its step, the times, the position and the depth, the counters | DONE | `core/job_card.hpp`, shown on the machine page. The tool block says number, diameter, place in the bar and hole (cut) X of N; the counters are the holes done (or the length routed) and the tool changes. What the server does not report is a dash, never a number. |
| Hour the job is expected to end at, in local time | DONE | Worked out from the time left. The clock is the system's when it is set, else the one the server reports (`time_s` of the fleet); the zone is `clock_utc_offset_min` (the chip has no time zones). Without any clock the hour is a dash. |
| Notice between the stages: the drilling is done and the routing comes next, with the buttons to follow the routing or to stop | DONE | `core/stage_notice.hpp`. A window with the two buttons (setting `stage_window`); "Follow the routing" goes back to the machine page, where the routing job waits for the operator's start through the gate; the stop button stops. |
| The order to take the board out only at the end of the whole job | DONE | The sound of the end and the line "take the board out" come only when the server said the whole job was done; the end of the drilling with the routing still to come is a stage of its own. |
| Voice of the stage (`stage.wav`) | DONE | The cue `Stage` has its own file in the folder of the voice chosen (`sound_stage = voice:es:f`) and a tone of its own when there is no recording. The recordings are the ones of the desktop program. |
| Panel to move the machine by hand: cross for X and Y, Z, steps 0.01, 0.1, 1, 10 and 50 mm, continuous mode, speeds, four places | DONE | `core/jog_panel.hpp`; the steps, speeds and clearances are in `hmi.cfg`. The moves wait for the link to the node, which is not written yet: until then the screen works the move out, asks the gate and says that nothing was sent (the position it keeps is not changed). |
| The panel is blocked while a job runs or the emergency stop is on | DONE | The gate refuses a move by hand while a job runs (switch `jog_block_running`) and always while the hardware stop is pressed or not acknowledged. |
| Limits of the travel and the safe distance of Z | DONE | The head stays inside the table (`jog_limits`), Z goes up to `jog_clearance_mm` before the head crosses the board (`jog_safe_z`), the tool does not go below the surface unless `jog_below_board_mm` allows it. Z does not move by hand while `jog_board_z_mm` is 0 (not known). |
| Keys of the panel (arrows, Page Up and Down, + and -, Esc) | N/A | The screen is a touch screen with no keyboard; the same actions are buttons and the stop is always on the bar. |
| The panel can be moved and docked | N/A | On 10.1 inches the panel has its own card; a floating panel could hide the stop. |
| Step, mode, speed and the four places kept between runs | MISSING | They are kept while the screen runs; the starting step, mode and speed come from `hmi.cfg`. Keeping the places in the flash is not written. |
| Emergency stop that can be moved and is always reachable | DIFFERS | The stop of the screen is fixed on the bar and always works; moving it would risk hiding it. The stop that protects people is the hardware button, which cuts the drive by itself. |
| Bar of the tool changer with every tool and its state | DONE | On a machine whose model says ATC the job page shows a square per tool with its place, in the colour of its state (pending, in use, done). |
| Window "Positions in the bar": a place for every tool, with validation | DONE | A drop-down list of places for every tool. A tool without a place, a place the machine does not define (`rack_places`) or two tools in one place stop the start and the screen says which tool and why (switch `rack_check`). The places are given in order (small drills first, then the cutters) until the operator changes them. |
| Places remembered for the next job with the same tools | MISSING | The places live while the screen runs. |
| More tools than places in the bar (loads of the rack) | MISSING | The desktop program splits the job into loads; the screen reports the tools without a place and does not start. Needs a decision on how the operator is asked to reload the bar. |
| Chained tool change of the drilling and the routing (`rack.first`, `carry_in`, `carry_out`, `slots`, `carry_slot`) | DONE | The request of the second version (`request_version` 2) carries the bar (switch `rack_chain`). A server that answers 400 or 422 gets the job again without it; one that answers without `rack_applied` is believed, and the screen shows in red that the tool changes were not chained. See `docs/SERVER_CONTRACT_NEEDS.md`. |
| Manual tool change: the spindle stops before each request to change the tool | N/A | The plan of the job, with the stops of the spindle (the original one and the variable-frequency drive), is made by the server with the engines of the drilling and routing programs; the screen only shows the request and answers "tool changed". |
| Panel of boards (plate, piece, common offset of drilling and routing) | N/A | The screen does not edit the design: the copies and the offset are in the files that the desktop program writes to the card or sends to the server. |
| Profiles of the axes in the 3D view | N/A | The screen has no 3D view; the viewer tab is still an empty card. |
| Themes | MISSING | The setting `theme` is read, but only the Forge theme is drawn; the other palettes come from a shared library of the desktop program and are not copied here. |
| Languages | DONE | The 37 languages, with the new texts in all of them (the translations of the 27 languages outside the eight of the tables, and the two variants, wait for a native review). |

## What the screen does not know

The screen does not reuse the logic of the desktop program: the rules above are written again in C++ and checked with the same cases. Whatever the server does not say (the head's
position, the feed, the turns of the spindle, the tool changes, the length routed) is shown as a dash.
