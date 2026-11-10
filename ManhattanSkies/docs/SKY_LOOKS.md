# Sky looks in Manhattan Skies

**The labels are my educated guesses.** The game has no friendly names for atmospheres, only asset paths, so each label is built from the file name (DLC prefixes: bc = Black Cat "The Heist", hh = Hammerhead "Turf Wars", ss = Silver Sable "Silver Lining"; "raft" is the prison; "feast" is the F.E.A.S.T. shelter; "cine" is a cutscene), the sky texture and the sun color.
The real file is in the third column; "what the name suggests" is inferred from the file name alone, not checked against the missions.
Numbers (sun rotation and elevation in degrees, RGB + intensity, sky texture) come from `recon/atmosphere_catalog.txt`.

| Style | Label (mine) | Real file (under `atmosphere/atm_ibl_lighting/`) | Sun rot/elev | Sun RGB, intensity | Sky texture | What the name suggests |
|---|---|---|---|---|---|---|
| DAY | CLASSIC | `atm_ibl_lgt_day_02.atmosphere` | 100.0 / 31.0 | 1.00,0.84,0.66,6.00 | 8k_Day_Sky_miles_cool2.texture | the open-world day |
| DAY | PAINTED SKY | `atm_ibl_lgt_day_02_cine.atmosphere` | 117.0 / 61.0 | 1.00,1.00,1.00,6.00 | 8k_day_sky_painted_2.texture | day, used by cutscenes |
| DAY | BRIGHT HDR | `atm_ibl_lgt_day_spidey_01.atmosphere` | 135.4 / 59.9 | 1.00,1.00,1.00,10.00 | day_skybox_hdr_02.texture | a Spider-Man hero-lighting day (intensity 10) |
| DAY | OPEN WORLD HDR | `atm_ibl_lgt_vn_scorpion_open_world.atmosphere` | 242.6 / 62.1 | 1.00,1.00,1.00,6.00 | day_skybox_hdr_02.texture | day for the Scorpion (VN_SCORPION) open-world content |
| DAY | CITY HALL | `atm_ibl_lgt_day_cityhall_destroy.atmosphere` | 171.0 / 63.8 | 1.00,1.00,1.00,1.20 | CGSkies_0129_day_4k_cityhall.texture | City Hall destruction scene |
| DAY | SOFT CLOUDS | `atm_cine_hh_theater_01.atmosphere` | 22.5 / 41.3 | 0.91,0.96,1.00,0.00 | cgskies_0027_4k_skybox_noBias.texture | a Hammerhead (Turf Wars DLC) theater cutscene |
| DAY | HAZY | `atm_milestease_vmfog.atmosphere` | 170.0 / 16.0 | 1.00,0.95,0.88,2.00 | skybox_ibl_cgskies_00012_free.texture | a Miles teaser with fog |
| DAY | FEAST DAY | `atm_feast_day_2018.atmosphere` | 230.0 / 52.0 | 1.00,1.00,1.00,1.00 | feast_2018_skybox.texture | the 2018 'feast' demo day |
| SUNSET | CLASSIC | `atm_ibl_lgt_sunset_02.atmosphere` | 185.3 / 1.2 | 1.00,0.26,0.00,1.00 | 8k_MainSunsetSky_NoSun_Moreclouds.texture | the open-world sunset |
| SUNSET | CLOUDY GOLD | `atm_ibl_lgt_sunset_infinitytrailer.atmosphere` | 188.0 / 25.0 | 1.00,0.61,0.30,1.50 | sunset_4k_cgs125_no_sun_nick_moreclouds.texture | an 'Infinity' trailer sunset |
| SUNSET | LOW SUN | `atm_cine_loadscreen_sunset_03.atmosphere` | 360.0 / 14.0 | 1.00,0.60,0.32,7.00 | sunset_4k_cgs125_no_sun_nick_moreclouds.texture | a loading-screen sunset |
| SUNSET | ORANGE GLOW | `atm_cine_antiserum_02.atmosphere` | 180.0 / 15.0 | 1.00,0.47,0.16,2.00 | 8k_MainSunsetSky_NoSun_Moreclouds.texture | the antiserum cutscene |
| SUNSET | 2018 SUNSET | `atm_2018_sunset.atmosphere` | 297.4 / 40.4 | 1.00,1.00,1.00,0.19 | feast_2018_skybox.texture | the 2018 demo sunset |
| SUNSET | GRAND CENTRAL | `atm_grand_central_sunset.atmosphere` | 345.0 / 30.0 | 1.00,1.00,1.00,1.00 | Sunset_4k_CGS125_No_Sun_GCT.texture | Grand Central Terminal sunset |
| SUNSET | PENTHOUSE | `sunset_normanpenthouse.atmosphere` | 136.0 / 18.0 | 1.00,1.00,1.00,0.75 | Sunset_4k_Norman_2018.texture | Norman Osborn's penthouse sunset |
| SUNSET | MAIN MENU | `atm_main_menu_sunset.atmosphere` | 230.5 / 14.0 | 1.00,0.61,0.30,1.17 | sunset_4k_cgs125_no_sun_nick_moreclouds.texture | the main menu sunset |
| SUNSET | HIGH SUN | `atm_cine_epilogue_mayfuneral_01.atmosphere` | 0.0 / 90.0 | 1.00,0.65,0.39,1.00 | sunset_4k_cgs125_no_sun_nick_moreclouds.texture | the epilogue, May's funeral |
| MORNING | OPENING | `atm_ibl_lgt_morning_01_ow.atmosphere` | 189.0 / 29.0 | 1.00,0.41,0.41,0.75 | 8k_Sky_EarlyMorning_01_OW.texture | the opening mission's dawn (the game's kBlackout) |
| MORNING | FISK | `atm_ibl_lgt_morning_01_fisk.atmosphere` | 148.0 / 29.0 | 1.00,0.71,0.60,0.75 | 8k_Sky_EarlyMorning_01_Fisk.texture | dawn at the Fisk scene |
| MORNING | PETER'S APARTMENT | `atm_peter_apt.atmosphere` | 170.0 / 23.5 | 1.00,0.74,0.74,0.75 | 8k_Sky_EarlyMorning_01.texture | Peter's apartment morning |
| NIGHT | CLASSIC | `atm_ibl_lgt_night_02.atmosphere` | 0.0 / 40.0 | 0.62,0.76,1.00,0.00 | 8k_NightSky1.texture | the open-world night |
| NIGHT | CLOUDY | `atm_ibl_lgt_night_02_old_forcines.atmosphere` | 234.0 / 70.0 | 0.62,0.76,1.00,0.02 | 4k_nightcloudy_nosunspot_1_hdr.texture | an older night used for cutscenes |
| NIGHT | STARRY | `atm_ibl_lgt_peterscook.atmosphere` | 234.8 / 70.0 | 0.54,0.70,1.00,0.02 | Sky_Night_3_32bit.texture | Peter cooking, a cutscene |
| NIGHT | MOONLIT | `atm_night_feast_2017.atmosphere` | 230.1 / 59.7 | 0.31,0.49,1.00,0.00 | night_sky_hdr_moon.texture | the 2017 'feast' demo night |
| NIGHT | BLUE HOUR | `atm_ottofightnight.atmosphere` | 155.0 / 15.0 | 0.17,0.27,0.58,0.03 | 4k_nightcloudy_nosunspot_1_hdr.texture | the Otto fight at night |
| NIGHT | DEEP BLUE | `atm_esu_night_interior.atmosphere` | 267.7 / 51.8 | 0.14,0.19,0.28,0.00 | cgskies_0324_night_4k_b.texture | ESU night |
| NIGHT | BANK NIGHT | `atm_night_shocker_bank_2017.atmosphere` | 220.0 / 25.0 | 0.71,0.71,1.00,0.10 | Sky_Night_2_16bit.texture | the Shocker bank mission (2017 build) |
| OVERCAST | CLASSIC | `atm_ibl_lgt_overcast_02.atmosphere` | 180.0 / 90.0 | 0.91,0.96,1.00,0.05 | overcast_skybox_hdr_04.texture | the open-world overcast |
| OVERCAST | DARK STORM | `atm_ibl_lgt_night_rain_02.atmosphere` | 234.0 / 70.0 | 0.54,0.70,1.00,0.01 | 4k_overcast_hdr_4b_scale.texture | the open-world night rain (dark overcast sky) |
| OVERCAST | GREEN HAZE | `atm_cine_rhinofight_outro_01.atmosphere` | 118.0 / 64.5 | 0.90,1.00,0.90,0.40 | overcast_skybox_hdr_04.texture | the Rhino fight outro |
| OVERCAST | LOW SUN | `atm_loadscreen_traversal_overcast_02.atmosphere` | 224.6 / -5.8 | 1.00,0.95,0.95,4.00 | overcast_skybox_hdr_04.texture | a traversal loading screen |
| STRANGE | SCORPION BLUE | `atm_ibl_lgt_day_scorpion_hallucination_02.atmosphere` | 320.0 / 12.0 | 0.58,0.25,0.03,3.00 | Scorpion_blue_small.texture | Scorpion hallucination (blue; its sky texture is Scorpion_blue_small) |
| STRANGE | SCORPION YELLOW | `atm_ibl_lgt_day_scorpion_hallucination_03.atmosphere` | 311.0 / 33.0 | 1.00,0.66,0.27,3.00 | scorpion_yellow_small.texture | Scorpion hallucination (yellow) |
| STRANGE | SCORPION GREEN | `atm_ibl_lgt_day_scorpion_hallucination_01.atmosphere` | 143.0 / 26.0 | 0.81,0.90,0.67,6.00 | day_skybox_hdr_02.texture | Scorpion hallucination 1 (a day sky with a green-tinted sun) |
| STRANGE | 1700S SUNSET | `test_atm_corrupt_biotech_nightmarelab.atmosphere` | 70.0 / 50.0 | 1.00,1.00,1.00,1.00 | skybox_1700_sunset_2017_all.texture | a 'test' atmosphere for the Biotech nightmare lab (1700s sunset sky) |
| STRANGE | CORRUPTION | `atm_corrupt_truck_chase.atmosphere` | 70.0 / 50.0 | 1.00,1.00,1.00,1.00 | (none listed) | the Negative corruption truck chase |
| STRANGE | GREEN NIGHT | `atm_cine_mj_investigates_standish_01.atmosphere` | 234.0 / 70.0 | 0.01,1.00,0.00,0.00 | 4k_nightcloudy_nosunspot_1_hdr.texture | an MJ investigation cutscene |
| DAY | FEAST CUTSCENE | `atm_feast_day_2018_cine.atmosphere` | 270.0 / 60.0 | 1.00,1.00,1.00,0.25 | feast_2018_skybox.texture |  |
| DAY | FLU MONTAGE SHOT 4 | `atm_lgt_cine_flumontage_shot04.atmosphere` | 43.2 / 61.0 | 1.00,1.00,1.00,6.00 | 8k_day_sky_painted_2.texture |  |
| DAY | FLU SHOT CUTSCENE | `atm_lgt_cin_flu_shot_01.atmosphere` | 32.0 / 65.0 | 1.00,1.00,1.00,7.00 | 8k_day_sky_painted_2.texture |  |
| DAY | HONORED CUTSCENE | `atm_ibl_lgt_day_jd_honored_cine_03.atmosphere` | 117.0 / 61.0 | 1.00,1.00,1.00,6.00 | 8k_Day_Sky_Painted_2.texture |  |
| DAY | LIGHTING DEMO DAY | `atm_gi_demo_menu_day.atmosphere` | 255.2 / 54.8 | 0.81,0.77,0.77,7.00 | day_skybox_hdr_02.texture |  |
| DAY | LOADING COMBAT DAY | `atm_load_combat_day.atmosphere` | 19.0 / 20.4 | 1.00,1.00,1.00,3.00 | day_skybox_hdr_02.texture |  |
| DAY | LOADING SCREEN COMBAT | `atm_ibl_lgt_day_loadingscreen_combat.atmosphere` | 117.0 / 61.0 | 1.00,1.00,1.00,6.00 | day_skybox_hdr_02.texture |  |
| DAY | MILES HELICOPTER | `atm_cine_heli_miles_01.atmosphere` | 117.0 / 61.0 | 1.00,0.94,0.88,6.00 | 8k_Day_Sky_Painted_2.texture |  |
| DAY | MJ TOMBSTONE GAMEPLAY | `atm_mj_tombstone_intro_gameplay.atmosphere` | 117.0 / 61.0 | 1.00,1.00,1.00,0.00 | 8k_day_sky_painted_2.texture |  |
| DAY | MJ TOMBSTONE INTRO | `atm_mj_tombstone_intro.atmosphere` | 117.0 / 61.0 | 1.00,1.00,1.00,0.00 | 8k_day_sky_painted_2.texture |  |
| DAY | PRISONER BASE | `atm_lgt_cine_shot02_prisoner_base.atmosphere` | 100.2 / 53.3 | 1.00,0.83,0.58,2.00 | 8k_day_sky_painted_2.texture |  |
| DAY | SANITARIUM (HAMMERHEAD) | `atm_lgt_hh_sanitarium_interior_01.atmosphere` | 22.5 / 41.3 | 0.91,0.96,1.00,0.00 | cgskies_0027_4k_skybox_noBias.texture |  |
| DAY | STILL IN LOVE RESTAURANT | `atm_cine_still_in_love_restaurant.atmosphere` | 60.0 / 61.0 | 1.00,1.00,1.00,6.00 | 8k_day_sky_painted_2.texture |  |
| DAY | TRUCK CHASE 2018 | `atm_truck_chase_2018.atmosphere` | 117.0 / 61.0 | 1.00,1.00,1.00,6.00 | 8k_day_sky_painted_2.texture |  |
| SUNSET | BLACK CAT RESCUE | `atm_ss_black_cat_save.atmosphere` | 188.0 / 15.0 | 1.00,0.61,0.30,1.50 | 8k_MainSunsetSky_NoSun_Moreclouds.texture |  |
| SUNSET | FLU MONTAGE SUNSET | `atm_lgt_cine_flumontage_shot_03.atmosphere` | 356.8 / 40.8 | 1.00,0.47,0.30,1.79 | sunset_4k_cgs125_no_sun_nick_moreclouds.texture |  |
| SUNSET | HAMMERHEAD AMBUSH A | `atm_ss_hammerheadambush_main_a.atmosphere` | 188.0 / 15.0 | 1.00,0.61,0.30,1.50 | 8k_MainSunsetSky_NoSun_Moreclouds.texture |  |
| SUNSET | HAMMERHEAD AMBUSH B | `atm_ss_hammerheadambush_main_b.atmosphere` | 188.0 / 15.0 | 1.00,0.61,0.30,1.50 | 8k_MainSunsetSky_NoSun_Moreclouds.texture |  |
| SUNSET | INFINITY WAR PHONE | `atm_sunset_infinitywar_phoneclose.atmosphere` | 189.8 / 13.7 | 1.00,0.58,0.42,0.60 | sunset_4k_cgs125_no_sun_nick_moreclouds.texture |  |
| SUNSET | NORMAN TEASER | `atm_norman_tease_cine.atmosphere` | 136.0 / 18.0 | 1.00,1.00,1.00,3.00 | Sunset_4k_Norman_2018.texture |  |
| SUNSET | PENTHOUSE CUTSCENE | `sunset_normanpenthouse_cine.atmosphere` | 136.0 / 18.0 | 1.00,1.00,1.00,0.75 | sunset_4k_norman_2018_cine.texture |  |
| SUNSET | SHOCKER BANK SUNSET | `atm_bank_shocker.atmosphere` | 220.0 / 25.0 | 1.00,1.00,1.00,3.00 | skybox_sunset_materials_test.texture |  |
| SUNSET | START MENU SUNSET | `start_menu_sunset_atmo.atmosphere` | 156.5 / 27.8 | 1.00,0.61,0.30,0.50 | 8k_MainSunsetSky_NoSun_Moreclouds.texture |  |
| NIGHT | BIOTECH BOSS FIGHT | `atm_biotech_boss_fight_atm.atmosphere` | 234.0 / 70.0 | 0.54,0.70,1.00,0.00 | 4k_nightcloudy_nosunspot_1_hdr.texture |  |
| NIGHT | BIOTECH BOSS OUTRO | `atm_biotech_boss_outro_cine.atmosphere` | 234.0 / 70.0 | 0.54,0.70,1.00,0.00 | 4k_nightcloudy_nosunspot_1_hdr.texture |  |
| NIGHT | BLACK CAT GALLERY | `atm_ibl_ctns_bc_gallery.atmosphere` | 0.0 / 40.0 | 0.62,0.76,1.00,0.00 | 8k_NightSky1.texture |  |
| NIGHT | BLACK CAT INTRO | `atm_cine_bc_intro.atmosphere` | 0.0 / 40.0 | 0.62,0.76,1.00,0.00 | 8k_NightSky1.texture |  |
| NIGHT | BLACK CAT PENTHOUSE | `atm_cine_bc_04_finale_bcpenthouse.atmosphere` | 0.0 / 40.0 | 0.62,0.76,1.00,0.00 | 8k_NightSky1.texture |  |
| NIGHT | DLC INTRO NIGHT | `atm_ibl_lgt_night_ctns_intro.atmosphere` | 0.0 / 40.0 | 0.62,0.76,1.00,0.00 | 8k_nightsky1.texture |  |
| NIGHT | ELECTRO FIGHT INTRO | `atm_cine_qte_electro_fight_intro_01.atmosphere` | 235.0 / 70.0 | 0.62,0.76,1.00,0.06 | 4k_nightcloudy_nosunspot_1_hdr.texture |  |
| NIGHT | EPILOGUE OTTO | `atm_cine_epilogue_otto_01.atmosphere` | 234.0 / 70.0 | 0.62,0.76,1.00,0.02 | 4k_nightcloudy_nosunspot_1_hdr.texture |  |
| NIGHT | EVICTED DESATURATED | `atm_ibl_lgt_night_02_evicted_cine_desaturated.atmosphere` | 234.0 / 70.0 | 0.62,0.76,1.00,0.02 | 4k_nightcloudy_nosunspot_1_hdr.texture |  |
| NIGHT | FEAST NIGHT CUTSCENES | `atm_night_feast_cinematics.atmosphere` | 160.0 / 80.0 | 0.71,0.71,1.00,0.01 | night_sky_hdr_no_moon.texture |  |
| NIGHT | FEAST ON FIRE | `atm_cine_night_feast_on_fire_01.atmosphere` | 234.0 / 70.0 | 1.00,1.00,1.00,0.00 | 4k_nightcloudy_nosunspot_1_hdr.texture |  |
| NIGHT | FEAST ON FIRE ENDING | `atm_cine_night_feast_on_fire_end_02.atmosphere` | 136.0 / 19.0 | 0.62,0.76,1.00,0.06 | 4k_nightcloudy_nosunspot_1_hdr.texture |  |
| NIGHT | GHOST (2018 DEMO) | `atm_ghost_2018.atmosphere` | 234.8 / 55.0 | 0.35,0.42,0.47,0.01 | Sky_Night_3_32bit.texture |  |
| NIGHT | HARLEM NIGHT | `atm_cine_har_night_02.atmosphere` | 234.0 / 70.0 | 0.62,0.76,1.00,0.02 | 4k_nightcloudy_nosunspot_1_hdr.texture |  |
| NIGHT | MICHAEL OPERATES | `atm_michaelsoperatescinematic.atmosphere` | 160.0 / 80.0 | 0.71,0.71,1.00,0.01 | night_sky_hdr_no_moon.texture |  |
| NIGHT | MJ APARTMENT NIGHT | `atm_mj_apartment_cine.atmosphere` | 135.0 / 55.0 | 0.00,0.00,0.00,0.00 | CGSkies_0324_night_4k_B.texture |  |
| NIGHT | OSCORP MISSION NIGHT | `atm_night_michaels_oscorp_mission.atmosphere` | 0.0 / 40.0 | 0.62,0.76,1.00,0.00 | 8k_NightSky1.texture |  |
| NIGHT | OSCORP OFFICE | `atm_ibl_oscorp_office_atmo.atmosphere` | 234.8 / 70.0 | 0.54,0.70,1.00,0.00 | 8k_nightsky1.texture |  |
| NIGHT | OTTO FIGHT OUTRO | `atm_ottofight_outro_cine.atmosphere` | 234.0 / 70.0 | 0.14,0.11,0.10,0.00 | 4k_nightcloudy_nosunspot_1_hdr.texture |  |
| NIGHT | OTTO RELEASED | `atm_otto_release_db.atmosphere` | 160.0 / 80.0 | 0.71,0.71,1.00,0.01 | night_sky_hdr_no_moon.texture |  |
| NIGHT | PETER COOKS | `atm_cine_petercooks_01.atmosphere` | 234.8 / 70.0 | 0.54,0.70,1.00,0.02 | Sky_Night_3_32bit.texture |  |
| NIGHT | PRISON INTRO NIGHT | `atm_ibl_lgt_night_prison_intro.atmosphere` | 0.0 / 40.0 | 0.62,0.76,1.00,0.00 | 8k_NightSky1.texture |  |
| NIGHT | RESTAURANT NIGHT | `atm_ibl_restaurant_cine_02.atmosphere` | 234.8 / 70.0 | 0.54,0.70,1.00,0.00 | Sky_Night_3_32bit.texture |  |
| NIGHTMORE | SHIPYARD OUTRO | `atm_ibl_lgt_shipyardoutro.atmosphere` | 234.8 / 70.0 | 0.54,0.70,1.00,0.02 | Sky_Night_3_32bit.texture |  |
| NIGHTMORE | SPIDEY AND BLACK CAT | `atm_cine_smbcat_intro.atmosphere` | 320.0 / 35.0 | 0.62,0.76,1.00,0.02 | 8k_NightSky1.texture |  |
| NIGHTMORE | TRAVERSAL LOADING | `atm_ibl_lgt_loadingtraversal.atmosphere` | 264.4 / 19.9 | 0.54,0.70,1.00,1.50 | Sky_Night_3_32bit.texture |  |
| NIGHTMORE | VULTURE DOWN | `atm_cine_vulture_down_01.atmosphere` | 114.3 / 27.9 | 0.62,0.76,1.00,0.10 | 4k_nightcloudy_nosunspot_1_hdr.texture |  |
| NIGHTMORE | VULTURE ELECTRO NODE | `atm_cine_vulture_electro_node_destruction_02.atmosphere` | 0.0 / 40.0 | 0.62,0.76,1.00,0.01 | 8k_NightSky1.texture |  |
| NIGHTMORE | VULTURE HATCH INSIDE | `atm_cine_vulture_investigate_hatch_int_01.atmosphere` | 0.0 / 40.0 | 0.62,0.76,1.00,0.00 | 8k_NightSky1.texture |  |
| NIGHTMORE | VULTURE HATCH OUTSIDE | `atm_cine_vulture_open_venthatch_ext_01.atmosphere` | 0.0 / 40.0 | 0.62,0.76,1.00,0.00 | 8k_NightSky1.texture |  |
| OVERCAST | FEAST OVERCAST | `feast_overcast_2018.atmosphere` | 180.0 / 90.0 | 0.91,0.96,1.00,0.05 | overcast_skybox_hdr_04.texture |  |
| OVERCAST | FUNERAL | `atm_ibl_lgt_overcast_funeral_cine_04.atmosphere` | 180.0 / 90.0 | 0.91,0.96,1.00,0.05 | overcast_skybox_hdr_04.texture |  |
| OVERCAST | LOADING INFILTRATION | `atm_loadscreen_infiltration.atmosphere` | 180.0 / 90.0 | 0.91,0.96,1.00,0.05 | overcast_skybox_hdr_04.texture |  |
| OVERCAST | PRISON HELI CRASH | `atm_cine_prison_heli_crash_01.atmosphere` | 234.0 / 70.0 | 0.54,0.70,1.00,0.01 | 4k_overcast_hdr_4b_scale_02.texture |  |
| OVERCAST | RAFT AFTER VULTURE | `atm_cine_post_vulture_raft_ext.atmosphere` | 90.0 / 60.0 | 0.54,0.70,1.00,0.01 | 4k_overcast_hdr_4b_scale_02.texture |  |
| OVERCAST | RAFT ENDING | `atm_cine_raft_ending_01.atmosphere` | 234.0 / 70.0 | 0.54,0.70,1.00,0.03 | 4k_overcast_hdr_4b_scale_02.texture |  |
| OVERCAST | RAFT NIGHT RAIN | `atm_ibl_lgt_night_rain_02_raft_ext.atmosphere` | 325.0 / 60.0 | 0.54,0.70,1.00,0.01 | 4k_overcast_hdr_4b_scale_02.texture |  |
| OVERCAST | RAFT VULTURE CAMEO | `atm_cine_raft_prison_vulture_cameo_03.atmosphere` | 234.0 / 70.0 | 0.54,0.70,1.00,0.01 | 4k_overcast_hdr_4b_scale_02.texture |  |
| OVERCAST | RHINO OUTRO | `atm_cine_rhino_miles_outro_01.atmosphere` | 180.0 / 90.0 | 0.91,0.96,1.00,0.07 | overcast_skybox_hdr_04.texture |  |
| OVERCAST | RHINO VS MILES | `atm_ibl_overcast_rhinomiles_02.atmosphere` | 180.0 / 90.0 | 0.91,0.96,1.00,0.05 | overcast_skybox_hdr_04.texture |  |
| OVERCAST | SABLE INTRO | `atm_sableintro_cinematic.atmosphere` | 180.0 / 90.0 | 0.91,0.96,1.00,0.05 | overcast_skybox_hdr_04.texture |  |
| OVERCAST | SANITARIUM OVERCAST | `atm_cine_overcast_sanitarium_partb.atmosphere` | 180.0 / 90.0 | 0.91,0.96,1.00,0.07 | overcast_skybox_hdr_04.texture |  |
| OVERCAST | SPEAKEASY OVERCAST | `atm_ibl_lgt_overcast_02_ctns_hh_speakeasy.atmosphere` | 180.0 / 90.0 | 0.91,0.96,1.00,0.05 | overcast_skybox_hdr_04.texture |  |
| OVERCAST | WATANABE TEACHES FEAR | `atm_cine_watanabeteachesfear.atmosphere` | 180.0 / 90.0 | 0.91,0.96,1.00,0.05 | overcast_skybox_hdr_04.texture |  |
| INTERIOR | ALT NIGHT (NO SKY) | `atm_alt_ibl_lgt_night.atmosphere` | 270.0 / 22.0 | 0.07,0.09,0.15,0.03 | (none listed) |  |
| INTERIOR | AUCTION HOUSE | `atm_ibl_auction_house.atmosphere` | 190.0 / 63.8 | 1.00,1.00,1.00,0.00 | black_bg_atmo.texture |  |
| INTERIOR | BIOTECH INTERIOR | `atm_night_biotech.atmosphere` | 160.0 / 80.0 | 1.00,1.00,1.00,0.00 | black_bg_atmo.texture |  |
| INTERIOR | BLACK CAT TRACKED | `atm_cine_bc_tracked_01.atmosphere` | 0.0 / 40.0 | 0.62,0.76,1.00,0.00 | (none listed) |  |
| INTERIOR | BLACK CAT TRACKED WARM | `atm_cine_bc_tracked_warm.atmosphere` | 0.0 / 40.0 | 0.62,0.76,1.00,0.00 | (none listed) |  |
| INTERIOR | CORRUPTION COMBAT | `atm_corrupt_truck_chase_combat.atmosphere` | 70.0 / 50.0 | 1.00,1.00,1.00,1.00 | (none listed) |  |
| INTERIOR | DEMON WAREHOUSE | `atm_ibl_lgt_night_demonwarehouse.atmosphere` | 80.0 / 80.0 | 0.07,0.09,0.15,0.03 | (none listed) |  |
| INTERIOR | GHOST FALL | `atm_ibl_ghost_fall_clip.atmosphere` | 267.7 / 51.8 | 0.14,0.19,0.28,0.10 | (none listed) |  |
| INTERIOR | LOADING NIGHT SWING | `atm_loadingnight_swinging_02.atmosphere` | 21.4 / 31.3 | 0.70,0.88,0.98,0.01 | (none listed) |  |
| INTERIOR | MAP SCREEN | `atm_nyc_map_2017.atmosphere` | 163.5 / 64.2 | 1.00,1.00,1.00,2.00 | (none listed) |  |
| INTERIOR | MARKETING CYBORG NIGHT | `atm_marketing_loadscreen_cyborg_night_02.atmosphere` | 21.4 / 31.3 | 0.70,0.88,0.98,0.01 | (none listed) |  |
| INTERIOR | MICHAEL'S INTERIOR | `atm_ibl_lgt_michaels_interior.atmosphere` | 117.0 / 61.0 | 1.00,1.00,1.00,0.00 | black_bg_atmo.texture |  |
| INTERIOR | OTTO HEALTH SCENE | `atm_ibl_cine_otto_health_cine_02.atmosphere` | 190.0 / 63.8 | 1.00,1.00,1.00,0.00 | black_bg_atmo.texture |  |
| INTERIOR | OTTO LEAVES | `atm_otto_leaves_cine.atmosphere` | 190.0 / 63.8 | 1.00,1.00,1.00,0.00 | black_bg_atmo.texture |  |
| INTERIOR | OTTO SUIT UPGRADE | `atm_ibl_lgt_otto_suit_upgrade_cine.atmosphere` | 190.0 / 63.8 | 1.00,1.00,1.00,0.00 | black_bg_atmo.texture |  |
| INTERIOR | OTTO'S LAB | `atm_ibl_lgt_otto.atmosphere` | 190.0 / 63.8 | 1.00,1.00,1.00,0.00 | black_bg_atmo.texture |  |
| INTERIOR | PAUSE MENU GADGETS | `atm_pausemenu_gadget.atmosphere` | 125.0 / 55.0 | 1.00,1.00,1.00,0.00 | (none listed) |  |
| INTERIOR | PAUSE MENU SUIT | `atm_pausemenu_paperdoll.atmosphere` | 0.0 / 55.0 | 1.00,1.00,1.00,0.00 | black_bg_atmo.texture |  |
| INTERIOR | PENTHOUSE NIGHT (NO SKY) | `atm_norman_penthouse_night.atmosphere` | 53.4 / 15.0 | 0.05,0.07,0.12,0.03 | (none listed) |  |
| INTERIOR | SHOCKER BANK (NO SKY) | `atm_bank_shocker_cinematic.atmosphere` | 220.0 / 25.0 | 1.00,0.76,0.62,3.00 | (none listed) |  |
