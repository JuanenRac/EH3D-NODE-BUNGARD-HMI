# This screen on the generic one

EH3D-NODE-HMI-GUI is the generic touch screen of the EH3D node family and the one that grows from now on. This repository is the same screen for one machine, the Bungard CCD drill and
router, and it is meant to become the generic screen with a settings profile. Nothing here is deleted until that is true.

* The profile is `profiles/bungard.cfg` of EH3D-NODE-HMI-GUI: the settings this screen carries on top of the generic ones (`machine_kind`, `tool_bar`, the `jog_*` panel and
  `clock_utc_offset_min`). `hmi.cfg.example` here is the generic file plus that profile.
* The core (`core/`), the host tests of the core, the link sockets, the board, the `sdkconfig` files and the texts of the 37 languages are the same files in both repositories; the
  firmware base of EH3D-COMMON copies them. Make a fix in the generic repository first.
* Written only here, and to be written in the generic screen before this repository can shrink to its profile: the panel that moves the machine by hand (`core/jog_panel.hpp` and its
  pages), the card under the progress bar (`core/job_card.hpp`) and their tests (`tests/test_parity.cpp`, `docs/PARITY_BUNGARD_GUI.md`).
* Written first in the generic repository and not copied here yet: the login of the application `hmi`, the table of refusals of the server by code in eight languages, the settings
  of the connection (`server_user`, `server_password`, `server_timeout_s`, `server_retries`, `server_mdns`) and the machines of the plugins on the fleet page.

The full list, with what is needed to close each line, is `docs/PROFILES.md` of EH3D-NODE-HMI-GUI, and `tools/check_profile_parity.py` there compares the two repositories.
