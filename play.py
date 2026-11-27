#!/usr/bin/env python3
"""
Heart of The Alien Redux 2.0.5.1 - cross-platform launcher

A small text setup screen that works on Linux, macOS and Windows.
It reads and writes the same play.cfg as play.ps1.

    ./play.sh                     setup screen
    ./play.sh --setup             setup screen (explicit)
    ./play.sh --fullscreen        start immediately with extra options
    python3 play.py --print-options
"""

import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
CFG_PATH = os.path.join(ROOT, "play.cfg")

DEFAULTS = {
    "renderer": "hd",
    "quality": "omniscale",
    "hdsmooth": 30,
    "upscale": 0,
    "smooth": "sharp",
    "filter": "off",
    "scale": 0,
    "integer": "auto",
    "fullscreen": "off",
    "sound": "on",
    "volume": 100,
    "iso": "Heart Of The Alien (U)",
    "music": "",
    "keyfile": "keys.txt",
    "room": 0,
    "skippassword": "on",
}

FIELDS = [
    ("renderer", "Renderer", "choice", [("hd", "HD (OpenGL)"), ("sdl", "SDL (classic)")]),
    ("quality", "HD filter", "choice", [("omniscale", "Omniscale"), ("nearest", "Sharp nearest")]),
    ("hdsmooth", "HD smoothing", "number", (0, 100, 5)),
    ("upscale", "Classic upscaling", "choice", [(0, "Auto"), (1, "1x"), (2, "2x"), (3, "3x"), (4, "4x")]),
    ("smooth", "Classic smoothing", "choice", [("sharp", "Sharp"), ("smooth", "Scale2x/3x")]),
    ("filter", "Classic filtering", "choice", [("off", "Off"), ("on", "Bilinear")]),
    ("scale", "Window scale", "choice", [(0, "Auto"), (2, "2x"), (3, "3x"), (4, "4x"), (5, "5x"), (6, "6x")]),
    ("integer", "Integer scaling", "choice", [("auto", "Auto (remembered)"), ("on", "On"), ("off", "Off")]),
    ("fullscreen", "Fullscreen", "choice", [("off", "No"), ("on", "Yes")]),
    ("sound", "Sound", "choice", [("on", "On"), ("off", "Off")]),
    ("volume", "Volume", "number", (0, 100, 10)),
    ("iso", "ISO prefix", "text", None),
    ("music", "Music prefix", "text", None),
    ("keyfile", "Key bindings", "text", None),
    ("room", "Start room", "choice", [(0, "Normal flow")] + [(i, "Room %d" % i) for i in range(1, 9)]),
    ("skippassword", "Skip password", "choice", [("off", "No"), ("on", "Yes")]),
]

ACTIONS = [
    ("__start", "START GAME"),
    ("__reset", "RESET DEFAULTS"),
    ("__exit", "SAVE AND EXIT"),
]

LINE = 76


def clear_screen():
    if not sys.stdout.isatty():
        return
    os.system("cls" if os.name == "nt" else "clear")


def fit(text, width):
    text = str(text)
    if len(text) <= width:
        return text
    if width <= 3:
        return text[:width]
    return text[: width - 3] + "..."


def load_config():
    cfg = dict(DEFAULTS)
    if os.path.isfile(CFG_PATH):
        try:
            with open(CFG_PATH, "r", encoding="utf-8-sig") as fp:
                saved = json.load(fp)
            if isinstance(saved, dict):
                for key in DEFAULTS:
                    if key in saved and saved[key] is not None:
                        cfg[key] = saved[key]
        except (OSError, ValueError) as exc:
            print("Could not read play.cfg: %s" % exc)
    return normalize(cfg)


def normalize(cfg):
    for key, _label, kind, spec in FIELDS:
        value = cfg.get(key, DEFAULTS[key])
        if kind == "choice":
            values = [item[0] for item in spec]
            if value not in values:
                cfg[key] = DEFAULTS[key]
        elif kind == "number":
            low, high, _step = spec
            try:
                value = int(value)
            except (TypeError, ValueError):
                value = DEFAULTS[key]
            cfg[key] = max(low, min(high, value))
        else:
            cfg[key] = str(value).strip()

    cfg["iso"] = iso_prefix(str(cfg["iso"]))
    return cfg


def save_config(cfg):
    try:
        with open(CFG_PATH, "w", encoding="utf-8-sig") as fp:
            json.dump(cfg, fp, indent=4)
    except OSError as exc:
        print("Could not save play.cfg: %s" % exc)


def display_value(key, cfg):
    for field_key, _label, kind, spec in FIELDS:
        if field_key != key:
            continue
        value = cfg.get(key, DEFAULTS[key])
        if kind == "choice":
            for item_value, item_name in spec:
                if value == item_value:
                    return item_name
            return spec[0][1]
        if kind == "number":
            return str(value)
        if not value:
            if key == "music":
                return "(same as ISO)"
            if key == "keyfile":
                return "(keys.txt)"
            return "(default)"
        return str(value)
    return ""


def find_exe():
    if os.name == "nt":
        names = ("alien.exe", "alien")
    else:
        names = ("alien", "alien.exe")

    for name in names:
        for candidate in (os.path.join(ROOT, name),
                          os.path.join(ROOT, "build", name)):
            if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
                return candidate
    return None


def iso_prefix(text):
    text = text.strip().strip('"').strip("'")
    if not text:
        return text
    if os.path.isfile(text) and text.lower().endswith(".iso"):
        return text[:-4]
    if os.path.isdir(text):
        try:
            names = sorted(name for name in os.listdir(text)
                           if name.lower().endswith(".iso"))
            if names:
                return os.path.join(text, names[0][:-4])
        except OSError:
            pass
    if text.lower().endswith(".iso"):
        return text[:-4]
    return text


def iso_path(cfg):
    prefix = str(cfg["iso"]).strip()
    if not prefix:
        prefix = DEFAULTS["iso"]
    if prefix.lower().endswith(".iso"):
        prefix = prefix[:-4]
    candidates = [prefix + ".iso"]
    if "/" not in prefix and "\\" not in prefix:
        candidates.append(os.path.join(prefix, prefix + ".iso"))
    for candidate in candidates:
        if os.path.isfile(candidate):
            return os.path.abspath(candidate)
    return None


def build_args(cfg, extra):
    args = []
    if cfg["renderer"] == "hd":
        args += ["--hd"]
        if cfg["quality"] == "nearest":
            args += ["--sharp"]
        args += ["--hd-smooth", str(cfg["hdsmooth"])]
    else:
        args += ["--no-hd"]
        if int(cfg["upscale"]) != 0:
            args += ["--upscale", str(cfg["upscale"])]
        if cfg["smooth"] == "smooth":
            args += ["--smooth"]
        if cfg["filter"] == "on":
            args += ["--filter"]

    if int(cfg["scale"]) != 0:
        args += ["--scale", str(cfg["scale"])]
    if cfg["integer"] == "on":
        args += ["--integer-scale"]
    elif cfg["integer"] == "off":
        args += ["--no-integer-scale"]
    if cfg["fullscreen"] == "on":
        args += ["--fullscreen"]
    if cfg["sound"] == "off":
        args += ["--no-sound"]
    args += ["--music-volume", str(int(cfg["volume"]))]
    args += ["--sfx-volume", str(int(cfg["volume"]))]

    if cfg["iso"]:
        args += ["--iso", str(cfg["iso"])]
    if cfg["music"]:
        args += ["--music", str(cfg["music"])]
    if cfg["keyfile"] and cfg["keyfile"] != "keys.txt":
        args += ["--keys", str(cfg["keyfile"])]
    if int(cfg["room"]) != 0:
        args += ["--room", str(int(cfg["room"]))]
    args += ["--skip-password" if cfg["skippassword"] == "on" else "--no-skip-password"]

    return args + list(extra)


def show_menu(cfg, message):
    clear_screen()
    print("+" + "-" * LINE + "+")
    print("| " + "HEART OF THE ALIEN REDUX 2.0.5.1 - SETUP / CONFIGURATION".ljust(LINE - 1) + "|")
    print("| " + "Options are saved in play.cfg. Enter a number to change it.".ljust(LINE - 1) + "|")
    print("+" + "-" * LINE + "+")
    print("   %-26s %-21s %-21s" % ("OPTION", "CURRENT", "DEFAULT"))
    print("   " + "-" * 71)

    for index, (key, label, _kind, _spec) in enumerate(FIELDS, 1):
        current = fit(display_value(key, cfg), 21)
        default = fit(display_value(key, DEFAULTS), 21)
        print("   %2d %-26s %-21s %-21s" % (index, fit(label, 26), current, default))

    print("   " + "-" * 71)
    offset = len(FIELDS) + 1
    for index, (_key, label) in enumerate(ACTIONS, offset):
        print("   %2d %s" % (index, label))

    print("+" + "-" * LINE + "+")
    found = iso_path(cfg)
    if found is None:
        print("   ISO: NOT FOUND")
    else:
        print("   ISO: " + found)
    print("   [s]tart   [r]eset defaults   [q]uit and save")
    if message:
        print("   " + message[:LINE])


def edit_choice(key, label, spec, cfg):
    current = cfg[key]
    print("")
    for index, (value, name) in enumerate(spec, 1):
        marker = "*" if value == current else " "
        print("  %s %d) %s" % (marker, index, name))
    try:
        answer = input("  %s [Enter keeps %s]: " % (label, display_value(key, cfg))).strip()
    except EOFError:
        return
    if not answer:
        return
    if answer.isdigit():
        index = int(answer) - 1
        if 0 <= index < len(spec):
            cfg[key] = spec[index][0]
            return
    for value, name in spec:
        if answer.lower() in (str(value).lower(), name.lower()):
            cfg[key] = value
            return
    print("  Unknown value.")


def edit_number(key, label, low, high, step, cfg):
    current = cfg[key]
    print("")
    print("  %s: current %s, range %d-%d, step %d" % (label, current, low, high, step))
    try:
        answer = input("  New value [Enter keeps %s]: " % current).strip()
    except EOFError:
        return
    if not answer:
        return
    try:
        value = int(answer)
    except ValueError:
        print("  Not a number.")
        return
    cfg[key] = max(low, min(high, value))


def edit_text(key, label, cfg):
    current = cfg[key]
    print("")
    print("  %s: current %s" % (label, display_value(key, cfg)))
    print("  Empty keeps the current value; type 'default' to restore it.")
    try:
        answer = input("  New value: ").strip().strip('"')
    except EOFError:
        return
    if not answer:
        return
    if answer.lower() == "default":
        cfg[key] = DEFAULTS[key]
        return
    if key == "iso":
        answer = iso_prefix(answer)
    cfg[key] = answer


def edit_field(index, cfg):
    key, label, kind, spec = FIELDS[index]
    if kind == "choice":
        edit_choice(key, label, spec, cfg)
    elif kind == "number":
        edit_number(key, label, spec[0], spec[1], spec[2], cfg)
    else:
        edit_text(key, label, cfg)


def start_game(cfg, extra):
    exe = find_exe()
    if exe is None:
        print("")
        print("ERROR: alien executable not found next to play.py or in build/.")
        return False

    if iso_path(cfg) is None:
        print("")
        print("WARNING: the game ISO was not found.")
        print("Looked for: %s.iso" % cfg["iso"])
        try:
            answer = input("Start anyway? (y/N): ").strip().lower()
        except EOFError:
            return False
        if not answer.startswith("y"):
            return False

    save_config(cfg)
    args = build_args(cfg, extra)
    print("")
    print("Starting: %s" % exe)
    print("Options : %s" % " ".join(args))
    print("")
    try:
        code = subprocess.call([exe] + args)
    except OSError as exc:
        print("Could not start the game: %s" % exc)
        return False
    if code != 0:
        print("The game exited with code %d." % code)
    return True


def main(argv):
    force_menu = "--setup" in argv
    extra = [arg for arg in argv if arg != "--setup"]

    if "--help" in extra:
        print(__doc__)
        return 0

    cfg = load_config()

    if "--print-options" in extra:
        extra = [arg for arg in extra if arg != "--print-options"]
        print(json.dumps(cfg, indent=4))
        print("ARGS: " + " ".join(build_args(cfg, extra)))
        return 0

    if "--save-options" in extra:
        save_config(cfg)
        return 0

    if "--show-menu" in extra:
        show_menu(cfg, "Press Ctrl+C or close the window to leave this preview.")
        return 0

    # Command line arguments start immediately, like play.bat / play.ps1.
    if extra and not force_menu:
        save_config(cfg)
        return 0 if start_game(cfg, extra) else 1

    if not sys.stdin.isatty() and not force_menu:
        save_config(cfg)
        return 0 if start_game(cfg, []) else 1

    message = "Enter a number to change an option, or s to start."
    try:
        while True:
            show_menu(cfg, message)
            try:
                answer = input("  > ").strip().lower()
            except EOFError:
                save_config(cfg)
                return 0

            if answer in ("s", "start"):
                if start_game(cfg, []):
                    try:
                        input("Press Enter to return to the setup screen...")
                    except EOFError:
                        return 0
                    message = "Settings are saved. Press s to play again."
            elif answer in ("r", "reset"):
                cfg = dict(DEFAULTS)
                message = "Defaults restored."
            elif answer in ("q", "quit", ""):
                save_config(cfg)
                return 0
            elif answer.isdigit():
                number = int(answer)
                if 1 <= number <= len(FIELDS):
                    edit_field(number - 1, cfg)
                    message = ""
                else:
                    index = number - len(FIELDS) - 1
                    if index == 0:
                        if start_game(cfg, []):
                            try:
                                input("Press Enter to return to the setup screen...")
                            except EOFError:
                                return 0
                    elif index == 1:
                        cfg = dict(DEFAULTS)
                        message = "Defaults restored."
                    elif index == 2:
                        save_config(cfg)
                        return 0
            else:
                message = "Unknown command."
    except KeyboardInterrupt:
        save_config(cfg)
        print("")
        return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

