# genpresets.py [--write]: the presets that ship with Manhattan Skies (base-game skies only, no atmosphere mods). Checks every value against the row labels in
# mod/src/sky.cpp, menu.cpp and looks_extra.inc, then prints the C++ lines for presets.cpp's kShipped table; with --write also writes the files into the player's
# Presets folder (Documents\Marvel's Spider-Man Remastered\ManhattanSkies\Presets).
import os
import re
import sys

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "mod", "src")

TIMES = ["DAY", "EARLY MORNING", "SUNSET", "NIGHT", "NIGHT (VARIANT)", "OVERCAST", "OVERCAST (VARIANT)"]
STYLES = {"DAY": "kDay", "SUNSET": "kSunset", "MORNING": "kMorning", "NIGHT": "kNight", "MORE NIGHT": "kNightMore", "OVERCAST": "kOvercast", "STRANGE": "kStrange", "INTERIOR": "kInterior"}
ROWS = {
    "color": ["AUTO", "WHITE", "WARM", "GOLDEN", "ORANGE", "RED", "PINK", "PURPLE", "BLUE", "CYAN", "GREEN"],
    "bright": ["AUTO", "25%", "50%", "75%", "150%", "200%", "300%", "500%", "OFF"],
    "fog": ["AUTO", "NONE", "25%", "50%", "150%", "300%", "600%"],
    "fogdist": ["AUTO", "25%", "50%", "150%", "300%", "600%"],
    "haze": ["AUTO", "NONE", "LIGHT", "MEDIUM", "HEAVY", "THICK"],
    "exposure": ["AUTO", "-3 STOPS", "-2 STOPS", "-1 STOP", "-0.5 STOP", "+0.5 STOP", "+1 STOP", "+2 STOPS", "+3 STOPS"],
    "filter": ["AUTO", "NEUTRAL", "WARM", "COOL", "GOLDEN", "ORANGE", "RED", "PINK", "PURPLE", "BLUE", "TEAL", "GREEN"],
    "clouds": ["AUTO", "ON", "OFF"],
    "fogcolor": ["AUTO", "WHITE", "NEUTRAL", "WARM", "GOLDEN", "ORANGE", "RED", "PINK", "PURPLE", "BLUE", "TEAL", "GREEN", "DARK"],
    "dir": ["AUTO"] + ["%d DEG" % (i * 15) for i in range(24)],
    "height": ["AUTO"] + ["%d DEG" % h for h in (-10, -5, 0, 5, 10, 15, 20, 30, 40, 50, 60, 70, 80, 90)],
}

# (file name, [(key, value), ...]); order: time, style, variant, then the rows
PRESETS = [
    ("Blue Hour", [("time", "NIGHT"), ("style", "NIGHT"), ("variant", "BLUE HOUR"), ("haze", "LIGHT"), ("filter", "COOL"), ("fogcolor", "BLUE")]),
    ("Moonlit Rooftops", [("time", "NIGHT"), ("style", "NIGHT"), ("variant", "MOONLIT"), ("bright", "150%"), ("clouds", "ON"), ("haze", "LIGHT")]),
    ("Starry Night", [("time", "NIGHT"), ("style", "NIGHT"), ("variant", "STARRY"), ("clouds", "OFF"), ("fog", "25%"), ("haze", "NONE"), ("exposure", "+0.5 STOP")]),
    ("Deep Blue", [("time", "NIGHT"), ("style", "NIGHT"), ("variant", "DEEP BLUE"), ("filter", "COOL"), ("exposure", "+1 STOP")]),
    ("Noir", [("time", "NIGHT"), ("filter", "NEUTRAL"), ("exposure", "-1 STOP"), ("fog", "150%"), ("fogcolor", "DARK"), ("haze", "NONE")]),
    ("Neon Dusk", [("time", "NIGHT"), ("color", "PURPLE"), ("filter", "PURPLE"), ("fogcolor", "PURPLE"), ("haze", "LIGHT")]),
    ("Fog Bank", [("time", "NIGHT"), ("fog", "600%"), ("fogdist", "25%"), ("fogcolor", "NEUTRAL"), ("haze", "HEAVY")]),
    ("Stormy Night", [("time", "OVERCAST"), ("style", "OVERCAST"), ("variant", "DARK STORM"), ("fog", "150%"), ("haze", "MEDIUM"), ("exposure", "-0.5 STOP"), ("filter", "COOL")]),
    ("Grey Morning", [("time", "OVERCAST"), ("exposure", "-0.5 STOP"), ("haze", "MEDIUM"), ("fogcolor", "NEUTRAL")]),
    ("Breaking Cloud", [("time", "OVERCAST"), ("style", "OVERCAST"), ("variant", "LOW SUN"), ("filter", "WARM"), ("haze", "LIGHT")]),
    ("Golden Morning", [("time", "EARLY MORNING"), ("style", "MORNING"), ("variant", "FISK"), ("color", "GOLDEN"), ("haze", "LIGHT"), ("filter", "WARM")]),
    ("Misty Dawn", [("time", "EARLY MORNING"), ("fog", "300%"), ("haze", "HEAVY"), ("fogcolor", "WHITE"), ("exposure", "+0.5 STOP"), ("clouds", "ON")]),
    ("Apartment Sunrise", [("time", "EARLY MORNING"), ("style", "MORNING"), ("variant", "PETER'S APARTMENT"), ("filter", "WARM"), ("haze", "NONE")]),
    ("Crisp Clear Day", [("time", "DAY"), ("style", "DAY"), ("variant", "BRIGHT HDR"), ("haze", "NONE"), ("fog", "25%"), ("fogdist", "300%"), ("exposure", "-0.5 STOP")]),
    ("Painted Sky", [("time", "DAY"), ("style", "DAY"), ("variant", "PAINTED SKY"), ("clouds", "ON"), ("filter", "WARM"), ("haze", "LIGHT")]),
    ("Hazy Summer", [("time", "DAY"), ("style", "DAY"), ("variant", "HAZY"), ("haze", "MEDIUM"), ("filter", "WARM"), ("exposure", "+0.5 STOP")]),
    ("Soft Clouds", [("time", "DAY"), ("style", "DAY"), ("variant", "SOFT CLOUDS"), ("clouds", "ON"), ("filter", "COOL")]),
    ("Ember Sunset", [("time", "SUNSET"), ("style", "SUNSET"), ("variant", "ORANGE GLOW"), ("color", "ORANGE"), ("fogcolor", "ORANGE"), ("haze", "LIGHT")]),
    ("Grand Central Dusk", [("time", "SUNSET"), ("style", "SUNSET"), ("variant", "GRAND CENTRAL"), ("filter", "WARM"), ("haze", "MEDIUM")]),
    ("Cloudy Gold", [("time", "SUNSET"), ("style", "SUNSET"), ("variant", "CLOUDY GOLD"), ("color", "GOLDEN"), ("bright", "150%")]),
    ("Long Shadows", [("time", "SUNSET"), ("style", "SUNSET"), ("variant", "LOW SUN"), ("height", "5 DEG"), ("filter", "GOLDEN"), ("bright", "150%")]),
    ("Toxic Green", [("time", "DAY"), ("style", "STRANGE"), ("variant", "SCORPION GREEN"), ("haze", "MEDIUM"), ("fogcolor", "GREEN")]),
    ("Fever Dream", [("time", "DAY"), ("style", "STRANGE"), ("variant", "SCORPION YELLOW"), ("filter", "ORANGE"), ("haze", "LIGHT")]),
]


def looks():
    out = {}
    for fn in ("sky.cpp", "looks_extra.inc"):
        for m in re.finditer(r'\{\s*(k[A-Za-z]+),\s*"([^"]+)"\s*,\s*"', open(os.path.join(SRC, fn), encoding="utf-8").read()):
            out.setdefault(m.group(1), set()).add(m.group(2))
    return out


def check():
    known = looks()
    bad = 0
    for name, kv in PRESETS:
        style = None
        for k, v in kv:
            if k == "time":
                ok = v in TIMES
            elif k == "style":
                style = STYLES.get(v)
                ok = style is not None
            elif k == "variant":
                ok = style is not None and v in known.get(style, set())
            else:
                ok = k in ROWS and v in ROWS[k]
            if not ok:
                print("BAD: %s: %s=%s" % (name, k, v))
                bad += 1
    return bad


if __name__ == "__main__":
    if check():
        sys.exit(1)
    print("all %d presets check out" % len(PRESETS))
    for name, kv in PRESETS:
        print('        { "%s.ini", "[preset]\\n%s" },' % (name, "".join("%s=%s\\n" % kv_ for kv_ in kv)))
    if "--write" in sys.argv:
        d = os.path.join(os.path.expanduser("~"), "Documents", "Marvel's Spider-Man Remastered", "ManhattanSkies", "Presets")
        os.makedirs(d, exist_ok=True)
        for name, kv in PRESETS:
            with open(os.path.join(d, name + ".ini"), "w", encoding="utf-8", newline="") as f:
                f.write("[preset]\n" + "".join("%s=%s\n" % kv_ for kv_ in kv))
        print("written to", d)
