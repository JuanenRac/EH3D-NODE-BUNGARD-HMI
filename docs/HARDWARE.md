# The module

The screen is built on the Guition JC8012P4A1C module:

| Part | What it is |
|---|---|
| Processor | ESP32-P4 with 16 MB of flash and 32 MB of external RAM |
| Wireless | ESP32-C6 for Wi-Fi and Bluetooth, reached through ESP-Hosted |
| Display | 10.1 inch, 800 x 1280, JD9365 controller on MIPI-DSI, capacitive touch |
| Sound | ES8311 codec, NS4150 speaker amplifier, ES7210 microphone input with echo cancellation |
| Storage | a card slot and a battery connector |

The unit this firmware is meant for has the **new** panel variant (the maker ships two firmware demos, one for each).

## Pins the maker's demo uses

| Function | Pins |
|---|---|
| I2C (codec, touch) | SDA 7, SCL 8 |
| I2S (codec) | MCLK 13, SCLK 12, LRCK 10, DOUT 9, DIN 11 |
| Display reset | 27 |
| Touch reset and interrupt | 22 and 21 (new panel) |

These come from the board support code of the maker's demo and are to be checked against the schematic of the unit before anything is connected. The stop input is not one of them: `HMI_HW_STOP_GPIO` in menuconfig names the pin, and `-1` means that no stop is wired, which the screen treats as "do not control the machine" while `hw_stop_required` is on.

## The stop

The emergency stop of a machine is a hardware button wired to cut the drive on its own. This screen is not part of that chain: it reads one input to show the state and to refuse moves, and nothing it does can clear a stop. See the README for the rules of the gate.

## Not done yet

The link to EH3D-SERVER or to the node of the machine, the speech recogniser and the speaker path, and the viewer, job, machines-and-stations and settings tabs.
