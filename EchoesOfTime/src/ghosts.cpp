#include "ghosts.h"

#include "entity.h"
#include "fnatives.h"
#include "gamethread.h"
#include "hero.h"
#include "herofade.h"
#include "log.h"
#include "memory.h"
#include "mute.h"
#include "scan.h"
#include "settings.h"
#include "spawn.h"

#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace ghosts {
    // Per-ghost state for the size/drift experiments: the ghost's birth transform rows and position, filled in by ApplyGhost on the game thread once the pose is set.
    struct Fx { std::atomic<bool> ready{ false }; float rows[12] = {}; float pos[3] = {}; };
    struct Ghost { uintptr_t entity = 0; ULONGLONG born = 0; uint8_t last_alpha = 255; std::shared_ptr<Fx> fx; };

    static std::atomic<int> g_mode{ 0 };
    static std::vector<Ghost> g_live;          // worker thread only
    static ULONGLONG g_last_spawn = 0, g_last_time = 0;
    static float g_last_pos[3] = {};
    static bool g_have_last = false;
    static float g_speed = 0;                  // smoothed, m/s
    static bool g_preloaded = false;
    static std::string g_preloaded_path;
    static uintptr_t g_material = 0;
    static std::string g_material_path;        // what g_material was looked up for
    static int g_fail_streak = 0;
    static unsigned g_spawned = 0, g_removed = 0;
    static double g_spawn_ms = 0, g_remove_ms = 0;
    static bool g_logged_first_alpha = false;
    static unsigned g_pose_ok = 0, g_pose_fail = 0;
    // Ghost time is game time: it advances only while the world does. The game keeps a clock (exe+5d6e7e4, the one QuantumBreak uses to detect menus):
    // it stands still in the pause menu, photo mode and loading screens, and runs slower in slow motion. While it stands still nothing ages, spawns or
    // is removed, so the ghosts stay where they are for photos.
    constexpr uintptr_t kClockRva = 0x5d6e7e4;
    static uint64_t g_clock = 0;
    static ULONGLONG g_game_ms = 0, g_prev_real = 0;
    static bool g_paused = false;
    static float g_last_ghost_pos[3] = {};
    static bool g_have_ghost_pos = false;
    static std::vector<uintptr_t> g_effect_mats;   // loaded materials of the current look (one, or several for RAINBOW)
    static std::string g_effect_name;          // what g_effect_mat was looked up for
    static int g_look_tries = 0;
    static std::atomic<bool> g_slowmo{ false };   // QuantumBreak slow motion is on (new ghosts only spawn then, when only_slowmo is set)

    // Effect presets: materials the game itself layers on a character as a post-composite overlay (flag 2). Which of them are loaded in a given
    // level is logged the first time ghosts are ready; one that is not loaded is skipped.
    struct Preset { const char* name; const char* path; };
    // Looks are SURFACE materials: the submit code (exe+1799f10 -> exe+1775ea0) swaps each submesh's material for the override, so the override has to be a
    // material a mesh can be drawn with. The first test used composite/post materials (the override list marked them composite=1), which never changed
    // the ghost. All of these exist in the archive (toctool check); the log says which are loaded in the current level.
    static const Preset kPresets[] = {
        { "hologram",  "materials/jn_fizzco_logo_holo/hologram_city_01.material" },
        { "scanlines", "materials/jn_fizzco_logo_holo/weather_station_frequency_L44_plane_scanlines_mat.material" },
        { "red",       "materials/light_red/light_red.material" },
        { "grey",      "materials/test_grey_medium/test_grey_medium.material" },
        { "distort",   "material/visualeffect/characters/hero/hero_melee_punch_distortion_trail/blackcat_trail_distortion_infinite.material" },
        { "fear",      "material/visualeffect/characters/hero/hero_melee_punch_distortion_trail/hero_suitpower_fearitself_trail.material" },
        { "highlight", "material/characters/hero/hero_spideysense_ar_attacker_highlight/hero_spideysense_ar_weapon_highlight.material" },
        // Plain colours (all found in the archive with toctool check, 2026-11-20; whether each is loaded in an area is logged).
        { "crimson",   "materials/test_red/test_red.material" },
        { "yellow",    "materials/test_yellow/test_yellow.material" },
        { "green",     "materials/test_green/test_green.material" },
        { "blue",      "materials/test_blue_light/test_blue_light.material" },
        { "darkblue",  "materials/dark_blue/dark_blue.material" },
        { "white",     "materials/test_white/test_white.material" },
        { "black",     "materials/test_black/test_black.material" },
        { "brown",     "materials/test_brown/test_brown.material" },
        { "own",       "" },   // no override: the model's own materials
    };
    // The paths a look needs: none (the model's own), one, or several (RAINBOW: each clone takes the next colour).
    static std::vector<std::string> PresetPaths(const std::string& name) {
        if (name == "rainbow") return { "materials/test_red/test_red.material", "materials/test_yellow/test_yellow.material", "materials/test_green/test_green.material", "materials/test_blue_light/test_blue_light.material" };
        for (auto& p : kPresets) if (name == p.name) { if (!p.path[0]) return {}; return { p.path }; }
        return { name };   // a .material path
    }

    int Mode() { return g_mode; }
    void SetSlowMoActive(bool on) { g_slowmo = on; }
    bool HasLive() { return !g_live.empty(); }

    static double NowMs() {
        LARGE_INTEGER f, q;
        QueryPerformanceFrequency(&f);
        QueryPerformanceCounter(&q);
        return (double)q.QuadPart * 1000.0 / (double)f.QuadPart;
    }

    // Model instance of an entity ([entity+0]; it is also the transform), or 0 if it is not valid ([+0x5f] != 0 means invalid).
    static uintptr_t ModelInst(uintptr_t entity) {
        uintptr_t mi = 0;
        uint8_t invalid = 1;
        if (!entity || !memory::Read(entity, mi) || !mi || !memory::Read(mi + 0x5f, invalid) || invalid) return 0;
        return mi;
    }

    // Alpha as FadeModelAlphaAction (exe+20511a0) writes it: byte [ModelInst+0xbe] = alpha * 255, then SetModified(mi, -1). Game thread.
    static bool WriteAlpha(uintptr_t entity, uint8_t a) {
        const uintptr_t mi = ModelInst(entity);
        if (!mi) return false;
        if (!memory::Write<uint8_t>(mi + 0xbe, a)) return false;
        return fnat::SetModified_(mi);
    }

    // The same for the entity's skin parts (if it has any).
    static void WriteAlphaParts(uintptr_t entity, uint8_t a) {
        const uintptr_t sm = entity::FindComponent(entity, "SkinManager");
        uint32_t n = 0;
        if (!sm || !memory::Read(sm + 0x4c, n) || n == 0 || n > 256) return;
        for (uint32_t i = 0; i < n; i++) for (unsigned v = 0; v < 2; v++) {
            const uintptr_t part = fnat::SkinPart_(sm, i, v);
            uint8_t invalid = 1;
            if (!part || !memory::Read(part + 0x5f, invalid) || invalid) continue;
            memory::Write<uint8_t>(part + 0xbe, a);
            fnat::SetModified_(part);
        }
    }

    // What a model instance's skeleton looks like (for the pose-copy research): model, joint count ([[model+0x200]+2]) and the joint-matrix array ([mi+0xd8]).
    static void LogJoints(const char* what, uintptr_t mi) {
        uintptr_t model = 0, skel = 0, mats = 0;
        uint16_t joints = 0;
        memory::Read(mi + 0x88, model);
        if (model) memory::Read(model + 0x200, skel);
        if (skel) memory::Read(skel + 2, joints);
        memory::Read(mi + 0xd8, mats);
        LOG("ghosts: %s model=%llx joints=%u joint mats=%llx", what, (unsigned long long)model, (unsigned)joints, (unsigned long long)mats);
    }

    // Joint count of a model instance ([[[mi+0x88]+0x200]+2]), 0 if unreadable.
    static unsigned JointCount(uintptr_t mi) {
        uintptr_t model = 0, skel = 0;
        uint16_t n = 0;
        if (!memory::Read(mi + 0x88, model) || !model || !memory::Read(model + 0x200, skel) || !skel || !memory::Read(skel + 2, n)) return 0;
        return n;
    }

    // Copies the hero's current pose onto a ghost (game thread): the 64-byte joint matrices ([mi+0xd8]) and the orientation rows of the transform
    // (right, up, forward: +0x00..+0x2f; the position stays where the ghost was spawned). The ghost has no animation of its own, so nothing
    // rewrites them: the ghost keeps the pose it was born with, which is the point.
    static bool CopyPose(uintptr_t hero_actor, uintptr_t ghost_mi, bool verbose) {
        uintptr_t hmi = 0, hmats = 0, gmats = 0;
        if (!memory::Read(hero_actor, hmi) || !hmi) return false;
        const unsigned hn = JointCount(hmi), gn = JointCount(ghost_mi);
        memory::Read(hmi + 0xd8, hmats);
        memory::Read(ghost_mi + 0xd8, gmats);
        if (!hn || hn != gn || !hmats || !gmats) {
            if (verbose) LOG("ghosts: pose copy not possible (hero joints=%u mats=%llx, ghost joints=%u mats=%llx)", hn, (unsigned long long)hmats, gn, (unsigned long long)gmats);
            return false;
        }
        std::vector<uint8_t> buf((size_t)hn * 64);
        if (!memory::SafeRead(hmats, buf.data(), buf.size()) || !memory::SafeWrite(gmats, buf.data(), buf.size())) return false;
        float rows[12];
        if (memory::SafeRead(hmi, rows, sizeof(rows))) memory::SafeWrite(ghost_mi, rows, sizeof(rows));
        if (verbose) LOG("ghosts: pose copied: %u joints, mats %llx -> %llx", hn, (unsigned long long)hmats, (unsigned long long)gmats);
        return true;
    }

    void RemoveAll() {
        for (auto& g : g_live) {
            const double t0 = NowMs();
            spawn::DespawnActor(g.entity);
            g_remove_ms += NowMs() - t0; g_removed++;
        }
        g_live.clear();
        g_have_ghost_pos = false;
    }

    void SetMode(int mode) {
        if (mode < 0 || mode > 2) mode = 0;
        g_mode = mode;
        LOG("ghosts: mode %d (%s)", mode, mode == 0 ? "off" : mode == 1 ? "while fast" : "always");   // the ghost thread removes them (Update)
    }

    static bool EnsureReady(const std::string& want) {
        const settings::S& s = settings::Get();
        if (!g_preloaded || g_preloaded_path != s.actor) {
            spawn::Preload(s.actor);
            g_preloaded = true; g_preloaded_path = s.actor;
            LOG("ghosts: preloading %s", s.actor.c_str());
        }
        if (!spawn::Ready(s.actor, 0)) return false;
        if (g_effect_name != want) {
            g_effect_name = want;
            g_effect_mats.clear();
            g_look_tries = 0;
        }
        // The look's materials may still be loading: look again on each spawn until they show up (about 6 s at the default spawn rate).
        const std::vector<std::string> paths = PresetPaths(want);
        if (!paths.empty() && g_effect_mats.size() < paths.size() && g_look_tries < 400) {
            // The material table gets an entry as soon as a load is requested, but the material is only usable once its asset reports loaded (byte [asset+1] == 1,
            // as for actors): applying it earlier did nothing in the 2026-11-20 test.
            auto found = std::make_shared<std::vector<uintptr_t>>(paths.size(), 0);
            gamethread::Run([found, paths] {
                for (size_t i = 0; i < paths.size(); i++) {
                    uintptr_t m = fnat::FindMaterial(paths[i].c_str(), true);
                    if (!m) { fnat::LoadMaterial(paths[i].c_str()); m = fnat::FindMaterial(paths[i].c_str(), true); }   // not loaded in this level: ask the game for it
                    uint8_t state = 0;
                    if (m && memory::Read(m + 1, state) && state == 1) (*found)[i] = m;
                }
            });
            g_effect_mats.clear();
            for (uintptr_t m : *found) if (m) g_effect_mats.push_back(m);
            g_look_tries++;
            if (g_effect_mats.size() == paths.size()) LOG("ghosts: look '%s': %zu material(s) loaded after %d check(s)", want.c_str(), paths.size(), g_look_tries);
            else if (g_look_tries % 20 == 0) LOG("ghosts: look '%s' still loading (%zu of %zu) after %d checks", want.c_str(), g_effect_mats.size(), paths.size(), g_look_tries);
        } else if (paths.empty() && g_look_tries == 0) {
            g_look_tries = 1;
            LOG("ghosts: look '%s' (the model's own materials)", want.c_str());
        }
        // Hold the first ghosts back until the chosen look is usable (a few seconds at most), so the trail never starts in the wrong look.
        if (!paths.empty() && g_effect_mats.empty() && g_look_tries < 150) return false;
        return true;
    }

    // What a ghost needs once its model is up. Applied from the game thread on the first tick after the spawn, and retried for a few ticks until the
    // model has its joint matrices: a ghost touched inside the spawn job itself (before the model initialised) lost its pose copy and its opacity
    // flicker in the 2026-11-20 test, so nothing is written to it until the model is ready.
    struct Ctx { uintptr_t look; uint8_t a0; float heading; bool pose, first; uintptr_t hero_actor; float pos[3]; bool hidden; std::shared_ptr<Fx> fx; };
    // The ghost actor carries a HeroLoadoutCloner: the visible suit may be skin-part model instances (SkinManager) rather than the primary model, which is what
    // OverrideAllMaterialsAction (exe+bea4d0) walks (exe+20c9f20 over every part, both variants). Applies the look (and the opacity) to each part; retried until
    // the cloner has built them.
    static void ApplyToParts(std::shared_ptr<Ctx> c, uintptr_t e, uint8_t alpha, int attempt) {
        const uintptr_t sm = entity::FindComponent(e, "SkinManager");
        uint32_t n = 0;
        if (sm) memory::Read(sm + 0x4c, n);
        if (!sm || n == 0 || n > 256) {
            if (attempt < 40) gamethread::Post([c, e, alpha, attempt] { ApplyToParts(c, e, alpha, attempt + 1); });
            else if (c->first) LOG("ghosts: no skin parts after %d ticks (SkinManager %llx, count %u)", attempt, (unsigned long long)sm, n);
            return;
        }
        int done = 0;
        for (uint32_t i = 0; i < n; i++) for (unsigned v = 0; v < 2; v++) {
            const uintptr_t part = fnat::SkinPart_(sm, i, v);
            uint8_t invalid = 1;
            if (!part || !memory::Read(part + 0x5f, invalid) || invalid) continue;
            if (c->look) fnat::AddMaterial_(part, c->look, 0);
            memory::Write<uint8_t>(part + 0xbe, alpha);
            fnat::SetModified_(part);
            done++;
        }
        if (c->first) LOG("ghosts: skin parts: %u listed, look and opacity applied to %d model instance(s) after %d tick(s)", n, done, attempt);
    }

    static void ApplyGhost(std::shared_ptr<Ctx> c, uintptr_t e, int attempt) {
        const uintptr_t mi = ModelInst(e);
        uintptr_t mats = 0;
        if (mi) memory::Read(mi + 0xd8, mats);
        const bool model_ready = mi && mats && JointCount(mi) != 0;
        if (!model_ready && attempt < 8) {   // not initialised yet: try again next tick
            gamethread::Post([c, e, attempt] { ApplyGhost(c, e, attempt + 1); });
            return;
        }
        if (!mi) return;   // never became valid (despawned already?)
        bool mat_ok = true, copied = false;
        if (c->look) mat_ok = fnat::AddMaterial_(mi, c->look, 0);
        if (c->look) {
            // The Holo Decoy sets bit 3 of the model's flag word (+0x80) on its model and on every skin part (exe+d31ac5, exe+d31b5f) right after giving it its composite
            // hologram material; all of our looks are composite-type materials (flag bit 12), which are drawn as an overlay only when the model carries it.
            uint32_t flags = 0;
            memory::Read(mi + 0x80, flags);
            memory::Write<uint32_t>(mi + 0x80, flags | 8u);
            if (c->first) LOG("ghosts: model flags +0x80 %08x -> %08x", flags, flags | 8u);
        }
        if (c->first && c->look) {   // read the override list back: did the material land?
            uint8_t count = 0; uintptr_t list = 0, mat = 0; uint32_t id = 0; uint8_t bit = 0, flag = 0;
            memory::Read(mi + 0x9f, count); memory::Read(mi + 0xa0, list);
            if (list) { memory::Read(list, id); memory::Read(list + 6, bit); memory::Read(list + 7, flag); memory::Read(list + 8, mat); }
            LOG("ghosts: override list count=%u entry0: id=%x composite=%u flag=%u material=%llx (wanted %llx)", count, id, bit, flag, (unsigned long long)mat, (unsigned long long)c->look);
        }
        if (c->pose) copied = CopyPose(c->hero_actor, mi, c->first);
        if (c->hidden) {   // it was spawned out of sight: bring it to where the hero was
            const float p[3] = { c->pos[0], c->pos[1], c->pos[2] };
            memory::SafeWrite(mi + 0x30, p, sizeof(p));
        }
        if (c->fx) {   // birth transform for the size/drift experiments (the pose copy has just set the rows)
            memory::SafeRead(mi, c->fx->rows, sizeof(c->fx->rows));
            memcpy(c->fx->pos, c->pos, sizeof(c->fx->pos));
            c->fx->ready = true;
        }
        const bool alpha_ok = WriteAlpha(e, c->a0);
        ApplyToParts(c, e, c->a0, 0);
        if (c->pose) { if (copied) g_pose_ok++; else if ((g_pose_fail++ % 50) == 0) LOG("ghosts: pose copy failed (%u so far, %u ok)", g_pose_fail, g_pose_ok); }
        if (c->first) LOG("ghosts: applied after %d tick(s): material=%s pose=%s alpha=%s", attempt, c->look ? (mat_ok ? "ok" : "FAILED") : "none", copied ? "copied" : "not copied", alpha_ok ? "set" : "FAILED");
    }

    static std::shared_ptr<Fx> g_last_fx;   // the Fx of the ghost SpawnAt just made (the caller stores it with the ghost)

    static uintptr_t SpawnAt(const hero::Info& h, bool verbose, const std::string& look_name, float alpha_f) {
        const settings::S& s = settings::Get();
        if (!EnsureReady(look_name)) { if (verbose) LOG("ghosts: actor not loaded yet"); return 0; }
        const float fl = std::sqrt(h.fwd[0] * h.fwd[0] + h.fwd[2] * h.fwd[2]);
        float fx = 0, fz = 1;
        if (fl > 0.01f) { fx = h.fwd[0] / fl; fz = h.fwd[2] / fl; }
        const float heading = std::atan2(fz, fx) * 180.0f / 3.14159265f + s.yaw_offset;   // transform row 2 = (cos t, ., sin t)
        spawn::Vec3 pos{ h.pos[0] - fx * s.back_offset, h.pos[1] + s.y_offset, h.pos[2] - fz * s.back_offset };
        auto ghost_fx = std::make_shared<Fx>();
        const uintptr_t look = g_effect_mats.empty() ? 0 : g_effect_mats[g_spawned % g_effect_mats.size()];
        auto ctx = std::make_shared<Ctx>(Ctx{ look, (uint8_t)(std::min(1.0f, std::max(0.0f, alpha_f)) * 255.0f), heading, s.pose_copy, g_spawned < 3, h.actor, { pos.x, pos.y, pos.z }, s.hide_spawn, ghost_fx });
        g_last_fx = ghost_fx;
        if (s.hide_spawn) pos.y += 1500.0f;   // out of sight until ApplyGhost has set the pose and moved it back
        const double t0 = NowMs();
        // The callback runs on the game thread inside the spawn job: it only queues the apply for the next tick (the earliest the model can be ready).
        const uintptr_t e = spawn::SpawnActor(s.actor, pos, heading, false, [ctx](uintptr_t ent) { gamethread::Post([ctx, ent] { ApplyGhost(ctx, ent, 0); }); });
        const double ms = NowMs() - t0;
        if (!e) { g_fail_streak++; LOG("ghosts: spawn failed (%d in a row)", g_fail_streak); return 0; }
        g_fail_streak = 0; g_spawned++; g_spawn_ms += ms;
        if (verbose || !g_logged_first_alpha) {
            LOG("ghosts: spawned entity=%llx in %.1f ms heading=%.0f", (unsigned long long)e, ms, heading);
            g_logged_first_alpha = true;
            LogJoints("hero model", h.transform);
        }
        return e;
    }

    // ---- Accelerated Decoy ----
    // The game's Holo Decoy actor is what enemies attack (its TargetData carries kDecoy, which the standard enemy targeting config ranks above the hero; confirmed in game
    // 2026-11-21). We spawn it where the hero stands, hide it, keep it from expiring (its SpiderHoloDecoy component explodes it once +0x88 passes MinLifetime and either
    // MaxLifetime +0x80, MaxEngageTime +0x84 or a health test +0x9c fires) and show an after-image ghost instead: the hero's exact pose in his own suit.
    static const char* const kDecoyActor = "equipment/projectile/proj_HoloDecoy_Deployed/proj_HoloDecoy_Deployed.actor";
    static std::atomic<bool> g_decoy_want{ false }, g_decoy_cancel{ false }, g_decoy_off{ false }, g_decoy_busy{ false };
    static ULONGLONG g_decoy_want_since = 0;
    static bool g_decoy_preloaded = false;
    static unsigned g_decoy_tries = 0;
    static std::atomic<unsigned> g_decoy_placed{ 0 };
    struct Decoy { uintptr_t visual = 0, target = 0; ULONGLONG born = 0, end_start = 0; float end_len = 0.0f; bool live = false; float pos[3] = {}; ULONGLONG last_check = 0; int respawns = 0; };
    static Decoy g_decoy;

    void DecoyPlace() { g_decoy_want_since = GetTickCount64(); g_decoy_busy = true; g_decoy_want = true; }
    bool DecoyBusy() { return g_decoy_busy; }
    unsigned DecoyPlacedCount() { return g_decoy_placed; }
    void DecoyShutdown() { if (g_decoy_busy) g_decoy_off = true; }

    // The game's decoy is hidden on every game tick for as long as the context is alive (its skin parts are built over several ticks and its own code rewrites things).
    // Hiding means: opacity 0, and the model's overlay flag (bit 3 of the flag word at +0x80, on the model and on each skin part) cleared: the blue hologram is a composite
    // overlay (the actor's StaticCompositeMaterial) that the decoy turns on with that bit, and opacity does not touch it. Also kept from expiring (see above).
    // With hide_spawn its transform's orientation rows are zeroed for its first kMoveInTicks ticks (a model of no size is not drawn): its first frames are an A-pose until
    // its animation starts, and moving it 1500 m up instead (tried 2026-11-21) seems to have kept enemies from targeting it. It stays where it is.
    struct TargetCtx { std::atomic<bool> alive{ true }; int ticks = 0; bool hide_spawn = true, hide = true, saved = false, restored = false; float rows[12] = {}; };
    constexpr int kMoveInTicks = 12;
    static void ClearOverlay(uintptr_t e) {
        const uintptr_t mi = ModelInst(e);
        if (mi) { uint32_t f = 0; if (memory::Read(mi + 0x80, f) && (f & 8u)) memory::Write<uint32_t>(mi + 0x80, f & ~8u); }
        const uintptr_t sm = entity::FindComponent(e, "SkinManager");
        uint32_t n = 0;
        if (!sm || !memory::Read(sm + 0x4c, n) || n == 0 || n > 256) return;
        for (uint32_t i = 0; i < n; i++) for (unsigned v = 0; v < 2; v++) {
            const uintptr_t part = fnat::SkinPart_(sm, i, v);
            uint32_t f = 0; uint8_t invalid = 1;
            if (!part || !memory::Read(part + 0x5f, invalid) || invalid || !memory::Read(part + 0x80, f) || !(f & 8u)) continue;
            memory::Write<uint32_t>(part + 0x80, f & ~8u);
        }
    }
    static void HideLoop(std::shared_ptr<TargetCtx> c, uintptr_t e) {
        if (!c->alive.load()) return;
        c->ticks++;
        if (c->hide) { WriteAlpha(e, 0); WriteAlphaParts(e, 0); ClearOverlay(e); }
        if (c->hide_spawn && !c->restored) {
            const uintptr_t mi = ModelInst(e);
            if (mi) {
                if (!c->saved) c->saved = memory::SafeRead(mi, c->rows, sizeof(c->rows));
                if (c->saved) {
                    if (c->ticks < kMoveInTicks) { float z[12]; for (int i = 0; i < 12; i++) z[i] = (i % 4 == 3) ? c->rows[i] : 0.0f; memory::SafeWrite(mi, z, sizeof(z)); }
                    else { memory::SafeWrite(mi, c->rows, sizeof(c->rows)); c->restored = true; }
                }
            }
        }
        const uintptr_t comp = entity::FindComponent(e, "SpiderHoloDecoy");
        if (comp) { memory::Write<float>(comp + 0x80, 1.0e9f); memory::Write<float>(comp + 0x84, 1.0e9f); memory::Write<float>(comp + 0x9c, 1.0e9f); }
        gamethread::Post([c, e] { HideLoop(c, e); });
    }

    static std::shared_ptr<TargetCtx> g_target_ctx;   // the loop state of the current game decoy

    static uintptr_t SpawnDecoyTarget(const float pos[3]) {
        if (g_target_ctx) g_target_ctx->alive.store(false);
        auto c = std::make_shared<TargetCtx>();
        g_target_ctx = c;
        c->hide = !settings::Get().decoy_show_target;
        c->hide_spawn = settings::Get().decoy_hide_spawn && c->hide;
        spawn::Vec3 p{ pos[0], pos[1], pos[2] };
        const uintptr_t e = spawn::SpawnActor(kDecoyActor, p, 0.0f, false, [c](uintptr_t ent) { HideLoop(c, ent); });
        if (e) {   // silence it: its voice lines are dropped by event id (mute.cpp); sounds on these objects too
            const uint32_t hd = entity::HandleOf(e);
            mute::SetObjects(hd, (uint64_t)hd + 1, (uint64_t)hd + 2, (uint64_t)e);
        }
        return e;
    }

    static void StopTarget() {
        if (g_target_ctx) g_target_ctx->alive.store(false);
        mute::Clear();
    }


    // The game's decoy plays a destruction effect (a blue hologram blink) when it goes: sent 1500 m up first so it is not seen.
    static void DespawnTarget(uintptr_t t) {
        if (settings::Get().decoy_hide_end) {
            gamethread::Run([t] { const uintptr_t mi = ModelInst(t); float p[3]; if (mi && memory::SafeRead(mi + 0x30, p, sizeof(p))) { p[1] += 1500.0f; memory::SafeWrite(mi + 0x30, p, sizeof(p)); } });
            Sleep(60);
        }
        spawn::DespawnActor(t);
    }

    static void DecoyKill() {
        herofade::Stop();
        if (g_decoy.target) { DespawnTarget(g_decoy.target); g_decoy.target = 0; }
        StopTarget();   // after the despawn: the hide loop must run to the end, or the hologram shows for a frame
        if (g_decoy.visual) {
            const uintptr_t v = g_decoy.visual;
            gamethread::Run([v] { WriteAlpha(v, 0); WriteAlphaParts(v, 0); });
            spawn::DespawnActor(v);
            g_decoy.visual = 0;
        }
        g_decoy.live = false;
        g_decoy_busy = g_decoy_want.load();
    }

    static void DecoyBeginEnd(ULONGLONG now, float seconds, const char* why) {
        if (g_decoy.end_start) return;
        g_decoy.end_start = now ? now : 1;
        g_decoy.end_len = seconds;
        if (g_decoy.target) { DespawnTarget(g_decoy.target); g_decoy.target = 0; }
        StopTarget();
        herofade::Stop();   // the player's flicker marks the decoy's life: solid again as soon as it ends
        // enemies go back to him at once; the copy flickers out after
        LOG("decoy: ending (%s)", why);
    }

    static void UpdateDecoy(const hero::Info& h, ULONGLONG now) {
        const settings::S& s = settings::Get();
        if (g_decoy_off.exchange(false)) {
            g_decoy_want = false; g_decoy_cancel = false;
            if (g_decoy.live) { LOG("decoy: removed (ability switched)"); DecoyKill(); }
            g_decoy_busy = false;
            return;
        }
        if (g_decoy_want) {
            if (GetTickCount64() - g_decoy_want_since > 10000) { g_decoy_want = false; g_decoy_busy = g_decoy.live; LOG("decoy: gave up (the actors did not load in 10 s)"); return; }
            if (!g_decoy_preloaded) { spawn::Preload(kDecoyActor); g_decoy_preloaded = true; }
            if (!spawn::Ready(kDecoyActor, 0)) return;
            if (g_decoy.live) { LOG("decoy: replacing the old one"); DecoyKill(); g_decoy_busy = true; }
            const uintptr_t v = SpawnAt(h, g_decoy_tries++ % 25 == 0, s.decoy_look, s.decoy_alpha);
            if (!v) return;   // the look is still loading: try again next tick
            const uintptr_t t = SpawnDecoyTarget(h.pos);
            g_decoy = Decoy{};
            g_decoy.visual = v; g_decoy.target = t; g_decoy.born = now; g_decoy.live = true; g_decoy.last_check = now;
            memcpy(g_decoy.pos, h.pos, sizeof(g_decoy.pos));
            g_decoy_want = false;
            LOG("decoy: placed at (%.1f, %.1f, %.1f): copy %llx, target %llx, look %s, %.1f s", h.pos[0], h.pos[1], h.pos[2], (unsigned long long)v, (unsigned long long)t, s.decoy_look.c_str(), s.decoy_life);
            g_decoy_placed++;
            herofade::Start(h.actor, s.decoy_player_flicker);
            return;
        }
        if (!g_decoy.live) { g_decoy_busy = false; return; }
        // Lifetime.
        const ULONGLONG age = now - g_decoy.born;
        if (!g_decoy.end_start && (float)age / 1000.0f >= s.decoy_life) DecoyBeginEnd(now, 0.8f, "time is up");
        // The game's decoy was lost (shot to pieces, or ended): put it back while the copy is up, so enemies keep a target.
        if (!g_decoy.end_start && now - g_decoy.last_check >= 1000) {
            g_decoy.last_check = now;
            if ((!g_decoy.target || !spawn::FindSyncFor(g_decoy.target)) && g_decoy.respawns < 20) {
                g_decoy.respawns++;
                LOG("decoy: the game's decoy is gone; spawning it again (%d)", g_decoy.respawns);
                g_decoy.target = SpawnDecoyTarget(g_decoy.pos);
            }
        }
        // Opacity: flickers in over the first 0.25 s, crackles while up (the DECOY FLICKER row), flickers out over the ending.
        const float base = std::min(1.0f, std::max(0.0f, s.decoy_alpha));
        float a = base * (1.0f - s.decoy_flicker * ((float)(rand() % 1000) / 1000.0f));
        const float r = (float)(rand() % 1000) / 1000.0f;
        if (age < 250 && r > (float)age / 250.0f) a = 0.0f;
        if (g_decoy.end_start) {
            const float prog = (float)(now - g_decoy.end_start) / 1000.0f / g_decoy.end_len;
            if (prog >= 1.0f) { LOG("decoy: gone"); DecoyKill(); return; }
            if (r < prog) a = 0.0f; else a *= 1.0f - 0.5f * prog;
        }
        const uint8_t a8 = (uint8_t)(a * 255.0f);
        const uintptr_t v = g_decoy.visual;
        { const uintptr_t vis = g_decoy.visual; gamethread::Run([vis, a8] { WriteAlpha(vis, a8); WriteAlphaParts(vis, a8); }); }
    }

    void Update() {
        const settings::S& s = settings::Get();
        const ULONGLONG real = GetTickCount64();
        uint64_t clock = 0;
        const bool readable = memory::Read(scan::FromRva(kClockRva), clock);
        const bool moved = !readable || clock != g_clock;   // unreadable: assume the game is running
        g_clock = clock;
        if (g_prev_real && moved) g_game_ms += real - g_prev_real;
        g_prev_real = real;
        if (!moved) {   // frozen (pause menu, photo mode, loading): leave everything exactly as it is
            if (!g_paused) LOG("ghosts: game frozen; ghosts held");
            g_paused = true;
            g_last_time = g_game_ms;
            return;
        }
        if (g_paused) { g_paused = false; LOG("ghosts: game running again"); }
        // Slow motion switched off: every ghost goes at once (opacity to 0 on the game thread first, so none is drawn another frame, then they are despawned).
        if (!g_live.empty() && settings::Get().only_slowmo && !g_slowmo) {
            std::vector<uintptr_t> all;
            for (auto& g : g_live) all.push_back(g.entity);
            gamethread::Run([all] { for (uintptr_t e : all) { WriteAlpha(e, 0); WriteAlphaParts(e, 0); } });
            RemoveAll();
            return;
        }
        const hero::Info h = hero::Read();
        const ULONGLONG now = g_game_ms;
        // Speed from the position change (a jump of more than 40 m is a teleport or a load: no trail across it).
        if (h.ok) {
            if (g_have_last && now > g_last_time) {
                const float dx = h.pos[0] - g_last_pos[0], dy = h.pos[1] - g_last_pos[1], dz = h.pos[2] - g_last_pos[2];
                const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
                const float dt = (float)(now - g_last_time) / 1000.0f;
                if (d > 40.0f) { RemoveAll(); g_speed = 0; }
                else g_speed = g_speed * 0.6f + (d / dt) * 0.4f;
            }
            memcpy(g_last_pos, h.pos, sizeof(g_last_pos));
            g_last_time = now; g_have_last = true;
        } else {
            g_have_last = false; g_speed = 0;
            if (!g_live.empty()) RemoveAll();   // no hero (loading, menu): whatever we spawned is stale
            if (g_decoy.live) { LOG("decoy: removed (no hero)"); DecoyKill(); }
            return;
        }
        if (g_decoy_busy) UpdateDecoy(h, now);

        const int mode = g_mode;
        if (mode == 0 && !g_live.empty()) { RemoveAll(); return; }
        // Fade and expire.
        const ULONGLONG life_ms = (ULONGLONG)(s.life * 1000.0f);
        struct FadeItem { uintptr_t entity; uint8_t alpha; float frac; float age_s; std::shared_ptr<Fx> fx; };
        std::vector<FadeItem> fades;
        std::vector<uintptr_t> dead;
        const bool fx_on = s.drift != 0.0f || s.size_end != 1.0f;
        for (auto& g : g_live) {
            const ULONGLONG age = now - g.born;
            if (age >= life_ms) { dead.push_back(g.entity); continue; }
            // Opacity by age: the newest ghost is the most solid. fade_curve (the OPACITY DECAY SCALE row) shapes it: 1 = straight line, above 1 = the older ghosts drop off
            // faster (a tight solid core and a ghostly tail), below 1 = they stay solid longer.
            const float frac = (float)age / (float)life_ms;
            const float t = std::pow(1.0f - frac, s.fade_curve);
            const float jitter = 1.0f - s.flicker * ((float)(rand() % 1000) / 1000.0f);   // crackling opacity
            const uint8_t a = (uint8_t)(std::min(1.0f, std::max(0.0f, s.alpha)) * t * jitter * 255.0f);
            if (a != g.last_alpha || fx_on) { g.last_alpha = a; fades.push_back({ g.entity, a, frac, (float)age / 1000.0f, g.fx }); }
        }
        if (!fades.empty()) {
            const float drift = s.drift, size_end = s.size_end;
            gamethread::Run([fades, drift, size_end] {
                for (auto& f : fades) {
                    if (f.fx && f.fx->ready && (drift != 0.0f || size_end != 1.0f)) {   // size and drift experiments: rewrite the model transform (rows scaled about the feet, position raised)
                        const uintptr_t mi = ModelInst(f.entity);
                        if (mi) {
                            const float k = 1.0f + (size_end - 1.0f) * f.frac;
                            float rows[12];
                            for (int i = 0; i < 12; i++) rows[i] = (i % 4 == 3) ? f.fx->rows[i] : f.fx->rows[i] * k;
                            const float p[3] = { f.fx->pos[0], f.fx->pos[1] + drift * f.age_s, f.fx->pos[2] };
                            memory::SafeWrite(mi, rows, sizeof(rows));
                            memory::SafeWrite(mi + 0x30, p, sizeof(p));
                        }
                    }
                    WriteAlpha(f.entity, f.alpha);
                    WriteAlphaParts(f.entity, f.alpha);
                }
            });
        }
        for (uintptr_t e : dead) {
            const double t0 = NowMs();
            spawn::DespawnActor(e);
            g_remove_ms += NowMs() - t0; g_removed++;
            g_live.erase(std::remove_if(g_live.begin(), g_live.end(), [&](const Ghost& g) { return g.entity == e; }), g_live.end());
        }

        // Spawn.
        if (mode == 0 || g_fail_streak >= 3) {
            if (g_fail_streak == 3) { LOG("ghosts: 3 spawn failures in a row; ghosts switched off until the mode is changed"); g_fail_streak++; g_mode = 0; }
            return;
        }
        // "While fast" needs min_speed; "always" has no speed floor, and a ghost is only added once the hero is `spacing`
        // metres from the last one, so standing still (or creeping) never piles ghosts up inside him.
        const bool moving = g_speed >= (mode == 2 ? 0.0f : s.min_speed) && (g_slowmo || !s.only_slowmo);   // after-images belong to slow motion
        float since = 1e9f;
        if (g_have_ghost_pos) {
            const float dx = h.pos[0] - g_last_ghost_pos[0], dy = h.pos[1] - g_last_ghost_pos[1], dz = h.pos[2] - g_last_ghost_pos[2];
            since = std::sqrt(dx * dx + dy * dy + dz * dz);
        }
        if (moving && since >= s.spacing && (int)g_live.size() < s.max_ghosts && now - g_last_spawn >= (ULONGLONG)(s.interval * 1000.0f)) {
            g_last_spawn = now;
            const uintptr_t e = SpawnAt(h, false, s.effect, s.alpha);
            if (e) { g_live.push_back({ e, g_game_ms, 255, g_last_fx }); memcpy(g_last_ghost_pos, h.pos, sizeof(g_last_ghost_pos)); g_have_ghost_pos = true; }
        }
    }

    std::string Status() {
        char buf[240];
        snprintf(buf, sizeof(buf), "ghosts mode %d, %zu live, speed %.1f m/s, spawned %u (avg %.1f ms), removed %u (avg %.1f ms), pose copies %u ok / %u failed", g_mode.load(), g_live.size(), g_speed,
                 g_spawned, g_spawned ? g_spawn_ms / g_spawned : 0.0, g_removed, g_removed ? g_remove_ms / g_removed : 0.0, g_pose_ok, g_pose_fail);
        return buf;
    }
}
