import os
# make_sky_looks_doc.py: writes docs/SKY_LOOKS.md, the table of SKY STYLE / SKY VARIANT labels next to the real atmosphere files they load.
# Reads the look table out of mod/src/sky.cpp and the numbers from recon/atmosphere_catalog.txt.
import re

root = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
src = open(root + r"\mod\src\sky.cpp", encoding="utf-8").read() + open(root + r"\mod\src\looks_extra.inc", encoding="utf-8").read()
styles = ["AUTO", "DAY", "SUNSET", "MORNING", "NIGHT", "MORE NIGHT", "OVERCAST", "STRANGE", "INTERIOR"]
looks = re.findall(r"\{ k(\w+), \"([^\"]+)\", \"([^\"]+)\" \}", src)

cat = {}
for line in open(root + r"\recon\atmosphere_catalog.txt", encoding="utf-8"):
    parts = line.split()
    if len(parts) < 7 or not parts[0].endswith(".atmosphere"):
        continue
    name = parts[0].split("/")[-1]
    cat[name] = parts

# What the file name suggests (inferred from the name only, not verified against the game's missions)
hint = {
    "atm_ibl_lgt_day_02": "the open-world day",
    "atm_ibl_lgt_day_02_cine": "day, used by cutscenes",
    "atm_ibl_lgt_day_spidey_01": "a Spider-Man hero-lighting day (intensity 10)",
    "atm_ibl_lgt_vn_scorpion_open_world": "day for the Scorpion (VN_SCORPION) open-world content",
    "atm_ibl_lgt_day_cityhall_destroy": "City Hall destruction scene",
    "atm_cine_hh_theater_01": "a Hammerhead (Turf Wars DLC) theater cutscene",
    "atm_milestease_vmfog": "a Miles teaser with fog",
    "atm_feast_day_2018": "the 2018 'feast' demo day",
    "atm_ibl_lgt_sunset_02": "the open-world sunset",
    "atm_ibl_lgt_sunset_infinitytrailer": "an 'Infinity' trailer sunset",
    "atm_cine_loadscreen_sunset_03": "a loading-screen sunset",
    "atm_cine_antiserum_02": "the antiserum cutscene",
    "atm_2018_sunset": "the 2018 demo sunset",
    "atm_grand_central_sunset": "Grand Central Terminal sunset",
    "sunset_normanpenthouse": "Norman Osborn's penthouse sunset",
    "atm_main_menu_sunset": "the main menu sunset",
    "atm_cine_epilogue_mayfuneral_01": "the epilogue, May's funeral",
    "atm_ibl_lgt_morning_01_ow": "the opening mission's dawn (the game's kBlackout)",
    "atm_ibl_lgt_morning_01_fisk": "dawn at the Fisk scene",
    "atm_peter_apt": "Peter's apartment morning",
    "atm_ibl_lgt_night_02": "the open-world night",
    "atm_ibl_lgt_night_02_old_forcines": "an older night used for cutscenes",
    "atm_ibl_lgt_peterscook": "Peter cooking, a cutscene",
    "atm_night_feast_2017": "the 2017 'feast' demo night",
    "atm_ottofightnight": "the Otto fight at night",
    "atm_esu_night_interior": "ESU night",
    "atm_night_shocker_bank_2017": "the Shocker bank mission (2017 build)",
    "atm_ibl_lgt_overcast_02": "the open-world overcast",
    "atm_ibl_lgt_night_rain_02": "the open-world night rain (dark overcast sky)",
    "atm_cine_rhinofight_outro_01": "the Rhino fight outro",
    "atm_loadscreen_traversal_overcast_02": "a traversal loading screen",
    "atm_ibl_lgt_day_scorpion_hallucination_02": "Scorpion hallucination (blue; its sky texture is Scorpion_blue_small)",
    "atm_ibl_lgt_day_scorpion_hallucination_03": "Scorpion hallucination (yellow)",
    "atm_ibl_lgt_day_scorpion_hallucination_01": "Scorpion hallucination 1 (a day sky with a green-tinted sun)",
    "test_atm_corrupt_biotech_nightmarelab": "a 'test' atmosphere for the Biotech nightmare lab (1700s sunset sky)",
    "atm_corrupt_truck_chase": "the Negative corruption truck chase",
    "atm_cine_mj_investigates_standish_01": "an MJ investigation cutscene",
}

out = ["# Sky looks in Manhattan Skies", "",
       "**The labels are my educated guesses.** The game has no friendly names for atmospheres, only asset paths, so each label is built from the file name (DLC prefixes: bc = Black Cat \"The Heist\", hh = Hammerhead \"Turf Wars\", ss = Silver Sable \"Silver Lining\"; \"raft\" is the prison; \"feast\" is the F.E.A.S.T. shelter; \"cine\" is a cutscene), the sky texture and the sun color.",
       "The real file is in the third column; \"what the name suggests\" is inferred from the file name alone, not checked against the missions.",
       "Numbers (sun rotation and elevation in degrees, RGB + intensity, sky texture) come from `recon/atmosphere_catalog.txt`.", "",
       "| Style | Label (mine) | Real file (under `atmosphere/atm_ibl_lighting/`) | Sun rot/elev | Sun RGB, intensity | Sky texture | What the name suggests |", "|---|---|---|---|---|---|---|"]
for style, label, file in looks:
    name = file.replace(".atmosphere", "")
    c = cat.get(file)
    rot = f"{c[2]} / {c[3]}" if c else "?"
    rgb = c[4] if c else "?"
    tex = c[7] if c and len(c) > 7 else "(none listed)"
    out.append(f"| {style.upper()} | {label} | `{file}` | {rot} | {rgb} | {tex} | {hint.get(name, '')} |")
out.append("")
open(root + r"\docs\SKY_LOOKS.md", "w", encoding="utf-8").write("\n".join(out))
print(len(looks), "looks written")
