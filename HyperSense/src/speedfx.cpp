#include "speedfx.h"

#include "hero.h"
#include "log.h"
#include "memory.h"
#include "scan.h"

#include <cstdint>
#include <cstring>

// How the game reaches the config (Hero::HeroTraversalTracker's update, exe+863ff0, read 2026-11-20 [RE]):
//   handle = u32 [tracker + 0x48]; obj = handle table lookup (the function at exe+16798f0: table [exe+7a44320], entries of 16 bytes {ptr, ?, generation u32 at +8},
//   count u32 [exe+7a44340], handle = generation << 20 | index);
//   data = [obj + 0x48]; config = [data + 0x40], or data + 0x1850 when that is null (exe+923950);
//   HeroSpeedFXConfig = config + 0x17b8: floats StageOneSpeed +8, StageTwoSpeed +0xc, MotionBlurBonusMaxSwing +0x10, ...NonSwing +0x14, RampSpeedMin +0x18, RampSpeedMax +0x1c, ExposureScaleMax +0x20.
namespace speedfx {
    namespace {
        constexpr uintptr_t kTableRva = 0x7a44320, kCountRva = 0x7a44340;
        constexpr uintptr_t kSpeedFxOffset = 0x17b8;
        uintptr_t g_data = 0;       // the HeroSpeedFXConfig we found
        float g_orig[7] = {};       // its original floats (+8 .. +0x20)
        bool g_ok = false;
        float g_blur = 1.0f, g_stage = 1.0f;   // what is currently written

        uintptr_t FindData() {
            const hero::Info h = hero::Read();
            if (!h.ok) return 0;
            const uintptr_t tracker = hero::Component(h.actor, "HeroTraversalTracker");
            uint32_t handle = 0;
            if (!tracker || !memory::Read(tracker + 0x48, handle)) return 0;
            const uint32_t idx = handle & 0xfffff, gen = handle >> 20;
            uintptr_t table = 0;
            uint32_t count = 0;
            if (!gen || !memory::Read(scan::FromRva(kTableRva), table) || !table || !memory::Read(scan::FromRva(kCountRva), count) || idx >= count) return 0;
            uint32_t entry_gen = 0;
            uintptr_t obj = 0, data = 0, cfg = 0;
            if (!memory::Read(table + (uintptr_t)idx * 16 + 8, entry_gen) || entry_gen != gen || !memory::Read(table + (uintptr_t)idx * 16, obj) || !obj) return 0;
            if (!memory::Read(obj + 0x48, data) || !data) return 0;
            if (!memory::Read(data + 0x40, cfg) || !cfg) cfg = data + 0x1850;
            return cfg + kSpeedFxOffset;
        }
    }

    void Update(bool on, float blur_mult, float stage_mult) {
        const float blur = on ? blur_mult : 1.0f, stage = on ? stage_mult : 1.0f;
        if (!g_ok && blur == 1.0f && stage == 1.0f) return;   // nothing to do and nothing to restore
        const uintptr_t data = FindData();
        if (!data) return;
        if (data != g_data) {
            g_data = data;
            g_ok = false;
            float f[7];
            if (memory::SafeRead(data + 8, f, sizeof(f))) {
                LOG("speedfx: config at %llx: stage %.2f/%.2f blur swing %.2f nonswing %.2f ramp %.2f..%.2f exposure %.2f", (unsigned long long)data, f[0], f[1], f[2], f[3], f[4], f[5], f[6]);
                // sanity: speeds and ramp bounds positive and ordered, otherwise this is not the config and nothing is written
                if (f[0] > 0 && f[0] < 500 && f[1] >= f[0] && f[1] < 500 && f[5] >= f[4] && f[5] < 500) { memcpy(g_orig, f, sizeof(f)); g_ok = true; g_blur = g_stage = 1.0f; }
                else LOG("speedfx: values do not look like HeroSpeedFXConfig; leaving it alone");
            }
        }
        if (!g_ok || (blur == g_blur && stage == g_stage)) return;
        float f[7];
        memcpy(f, g_orig, sizeof(f));
        f[0] *= stage; f[1] *= stage;                                 // StageOneSpeed, StageTwoSpeed
        f[2] *= blur; f[3] *= blur; f[6] *= blur;                     // blur bonus swing / non-swing, exposure scale max
        memory::SafeWrite(data + 8, f, sizeof(f));
        g_blur = blur; g_stage = stage;
        LOG("speedfx: blur x%.1f, stage speeds x%.2f (%s)", blur, stage, on ? "on" : "restored");
    }
}
