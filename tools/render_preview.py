#!/usr/bin/env python3
# =============================================================================
# EH3D-NODE-BUNGARD-HMI - tools/render_preview.py
# Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
# GPL-3.0-or-later - see LICENSE
# =============================================================================
"""Draws a preview of the screens of the touch display from the texts of core/ui_text.hpp and the layout of main/ui.cpp.

    python tools/render_preview.py            # writes images/screenshots/screen-*.svg
    python tools/render_preview.py --png      # also renders them to PNG with Edge (needs Microsoft Edge)

It is a drawing of the layout, NOT a capture of a module: the firmware has not been run on one. The texts are the ones the firmware
shows, read from the header, so a change of a text shows up here.
"""
from __future__ import annotations

import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "images" / "screenshots"
LANGS = ["en", "es", "it", "de", "fr", "pt", "zh", "ja"]
W, H = 800, 1280
FONT = "'Segoe UI','Noto Sans','Noto Sans CJK SC','Noto Sans CJK JP','Yu Gothic',sans-serif"

NAMES = ["Viewer", "Job", "Machine", "Settings", "Fleet", "Start", "Pause", "Stop", "Home", "Confirm", "Cancel", "Armed", "NotArmed",
         "StopPressed", "AckRelease", "Language", "Voice", "Brightness", "Volume", "JogX", "JogY", "JogZ", "Spindle", "AskMove",
         "AskSpindle", "WhyHardwareStop", "WhyNotArmed", "WhyVoiceOff", "WhyNoStop", "WhySpindle"]


def texts() -> dict[str, list[str]]:
    src = (ROOT / "core" / "ui_text.hpp").read_text(encoding="utf-8")
    rows = re.findall(r'^\s*\{("(?:[^"\\]|\\.)*"(?:,\s*"(?:[^"\\]|\\.)*"){7})\},?$', src, re.M)
    out = {}
    for name, row in zip(NAMES, rows):
        out[name] = [s.replace('\\"', '"') for s in re.findall(r'"((?:[^"\\]|\\.)*)"', row)]
    if len(out) != len(NAMES):
        raise SystemExit(f"read {len(out)} rows of text, expected {len(NAMES)}")
    return out


def esc(s: str) -> str:
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def button(x, y, w, h, label, fill, size=28, color="#ffffff"):
    return (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="14" fill="{fill}"/>'
            f'<text x="{x + w / 2}" y="{y + h / 2 + size / 3}" font-size="{size}" fill="{color}" text-anchor="middle">{esc(label)}</text>')


def screen(lang: str, state: str) -> str:
    t = {k: v[LANGS.index(lang)] for k, v in texts().items()}
    stop_pressed = state == "stop"
    armed = state != "stop"
    g = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" font-family="{FONT}">',
         f'<rect width="{W}" height="{H}" fill="#15110c"/>']
    # the top: the stop button, the state, the acknowledgement of a released stop
    g.append('<rect x="10" y="10" width="780" height="150" rx="14" fill="#211b13" stroke="#3a2f20"/>')
    g.append(button(24, 30, 220, 110, t["Stop"], "#c62828", 36))
    g.append(f'<text x="270" y="78" font-size="26" fill="#e8dcc8">{esc(t["Armed"] if armed else t["NotArmed"])}</text>')
    if stop_pressed:
        g.append(f'<text x="270" y="122" font-size="22" fill="#ff5252">{esc(t["StopPressed"])}</text>')
    # the tabs
    tabs = ["Machine", "Job", "Viewer", "Fleet", "Settings"]
    g.append('<rect x="10" y="176" width="780" height="70" fill="#211b13"/>')
    tw = 780 / len(tabs)
    for i, k in enumerate(tabs):
        x = 10 + i * tw
        sel = i == 0
        g.append(f'<text x="{x + tw / 2}" y="220" font-size="18" fill="{"#ffb300" if sel else "#a89878"}" text-anchor="middle">{esc(t[k])}</text>')
        if sel:
            g.append(f'<rect x="{x + 20}" y="240" width="{tw - 40}" height="5" fill="#ffb300"/>')
    # the machine tab: a grid of buttons
    labels = [(f"{t['JogX']} -", "#37474f"), (f"{t['JogX']} +", "#37474f"), (f"{t['JogY']} -", "#37474f"), (f"{t['JogY']} +", "#37474f"),
              (f"{t['JogZ']} -", "#37474f"), (f"{t['JogZ']} +", "#37474f"), (t["Home"], "#37474f"), (t["Spindle"], "#37474f")]
    for i, (lab, col) in enumerate(labels):
        x = 30 + (i % 3) * 250
        y = 280 + (i // 3) * 150
        g.append(button(x, y, 220, 110, lab, col, 28))
    g.append(button(30, 280 + 3 * 150, 220, 110, t["Start"], "#2e7d32", 30))
    # the line of the gate
    if state == "refused":
        g.append(f'<text x="30" y="1180" font-size="22" fill="#ffb300">{esc(t["WhyHardwareStop"])}</text>')
    if state == "stop":
        g.append(button(270, 280 + 3 * 150, 330, 110, t["AckRelease"], "#f9a825", 24, "#1b1b1b"))
        g.append(f'<text x="30" y="1180" font-size="22" fill="#ffb300">{esc(t["WhyHardwareStop"])}</text>')
    if state == "confirm":
        g.append(f'<rect width="{W}" height="{H}" fill="#000" opacity="0.55"/>')
        g.append('<rect x="80" y="450" width="640" height="340" rx="18" fill="#211b13" stroke="#ffb300"/>')
        g.append(f'<text x="400" y="530" font-size="30" fill="#e8dcc8" text-anchor="middle">{esc(t["AskMove"])}</text>')
        g.append(f'<text x="400" y="580" font-size="26" fill="#a89878" text-anchor="middle">{esc(t["JogX"])} +</text>')
        g.append(button(110, 650, 220, 110, t["Confirm"], "#2e7d32", 30))
        g.append(button(470, 650, 220, 110, t["Cancel"], "#546e7a", 30))
    g.append("</svg>")
    return "\n".join(g)


SHOTS = [("screen-machine", "en", "idle"), ("screen-confirm", "es", "confirm"), ("screen-stop", "ja", "stop")]


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    for name, lang, state in SHOTS:
        (OUT / f"{name}.svg").write_text(screen(lang, state), encoding="utf-8", newline="\n")
        print("written", name)
    if "--png" in sys.argv:
        edge = next((p for p in (r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe", r"C:\Program Files\Microsoft\Edge\Application\msedge.exe") if Path(p).exists()), None)
        if not edge:
            print("Microsoft Edge not found")
            return 1
        with tempfile.TemporaryDirectory() as tmp:
            for name, _, _ in SHOTS:
                subprocess.run([edge, "--headless=new", f"--user-data-dir={tmp}", "--hide-scrollbars", f"--window-size={W},{H}",
                                f"--screenshot={OUT / (name + '.png')}", (OUT / f"{name}.svg").as_uri()], capture_output=True, check=False)
                print("rendered", name)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
