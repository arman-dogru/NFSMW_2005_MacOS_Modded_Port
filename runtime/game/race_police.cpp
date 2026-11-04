// SpeedBreaker. GPL-3.0-or-later (see COPYING).
// Opt-in USA race police experiment. See docs/NATIVE_MODS.md for evidence
// and the distinction between race eligibility and observed spawning.
#include <stdafx.h>
#include "race_police.h"
#include "police_probe.h"
#include <region/region.h>
#include <charconv>
#include <kernel/heap.h>
#include <kernel/memory.h>

extern "C" PPC_FUNC(__imp__sub_823380E0);
extern "C" PPC_FUNC(__imp__sub_8233A738);
extern "C" PPC_FUNC(__imp__sub_820F18F8);
extern "C" PPC_FUNC(__imp__sub_820F21A8);
extern "C" PPC_FUNC(__imp__sub_820EB6A8);

namespace
{
    const bool s_enabled = [] {
        const char* v = std::getenv("NFSMW_RACE_POLICE");
        return v && std::strcmp(v, "1") == 0;
    }();

    unsigned Option(const char* name, unsigned fallback, unsigned maximum, bool allowZero = false)
    {
        const char* v = std::getenv(name);
        if (!v)
            return fallback;
        unsigned value;
        const auto result = std::from_chars(v, v + std::strlen(v), value);
        if (result.ec != std::errc{} || *result.ptr || (!allowZero && value < 1) || value > maximum)
        {
            fprintf(stderr, "[race-police] invalid %s; using %u\n", name, fallback);
            return fallback;
        }
        return value;
    }
    const unsigned s_density = Option("NFSMW_POLICE_DENSITY", 3, 3);
    const unsigned s_delay = Option("NFSMW_POLICE_START_DELAY", 3, 60, true);

    bool Enabled() { return s_enabled && region::Current() == region::Release::Usa; }

    uint32_t Read32(const uint8_t* base, uint32_t addr)
    {
        uint32_t n;
        std::memcpy(&n, base + addr, 4);
        return ByteSwap(n);
    }

    bool ActiveRace(const uint8_t* base)
    {
        const uint32_t race = Read32(base, 0x82A2CB18);
        return race && Read32(base, race + 0x1A28) == 1 && Read32(base, race + 0x1A30);
    }

    void LimitInitialDelay(uint8_t* base, uint32_t managerInterface)
    {
        if (!Enabled() || !ActiveRace(base)) return;
        const uint32_t bits = Read32(base, managerInterface + 0x58);
        float original;
        std::memcpy(&original, &bits, 4);
        if (!std::isfinite(original) || original <= float(s_delay)) return;
        const float seconds = float(s_delay);
        uint32_t stored;
        std::memcpy(&stored, &seconds, 4);
        stored = ByteSwap(stored);
        std::memcpy(base + managerInterface + 0x58, &stored, 4);
        fprintf(stderr, "[race-police] initial spawn cooldown %.2f -> %.2f seconds\n", original, seconds);
    }

    uint32_t AttributeValue(unsigned field)
    {
        // Private guest-readable values, never edits the loaded VLT database
        // or its shared fallback-zero object. The runtime heap is initialized
        // before any guest attribute call. Function-local static is thread safe.
        static const uint32_t values = [] {
            auto* p = static_cast<uint8_t*>(g_userHeap.Alloc(8));
            if (!p) return uint32_t(0);
            std::memset(p, 0, 8);
            p[0] = 1; // CopsInRace is a byte attribute, not a big-endian int.
            const uint32_t density = ByteSwap(s_density == 3 ? 100u : s_density == 2 ? 66u : 33u);
            std::memcpy(p + 4, &density, 4);
            return g_memory.MapVirtual(p);
        }();
        return values ? values + field : 0;
    }

    void Log(const char* name, double before, double after)
    {
        // Bounded diagnostic output, including frontend serialization calls.
        static unsigned lines = 0;
        if (lines++ < 24)
            fprintf(stderr, "[race-police] %s %.2f -> %.2f\n", name, before, after);
    }
}

bool game::race_police::CanSpawn(const uint8_t* base, uint32_t managerInterface, uint32_t argument, bool original)
{
    if (original || !Enabled()) return original;
    // The original CanSpawn uses this global as a blanket suppression during
    // races. Bypass that one condition only in an initialized active race;
    // keep the manager's block, spawn cooldown and cinematic/script gates.
    if (!ActiveRace(base) ||
        !Read32(base, 0x82A2BD94) || Read32(base, 0x82C5E750) ||
        (base[managerInterface + 0x60] && !(argument & 0xFF))) return false;
    const uint32_t bits = Read32(base, managerInterface + 0x58);
    float cooldown;
    std::memcpy(&cooldown, &bits, 4);
    return std::isfinite(cooldown) && cooldown <= 0;
}

PPC_FUNC(sub_820EB6A8)
{
    const uint32_t managerInterface = ctx.r3.u32, argument = ctx.r4.u32;
    __imp__sub_820EB6A8(ctx, base);
    if (!ctx.r3.u32 && game::race_police::CanSpawn(base, managerInterface, argument, false))
        ctx.r3.u64 = 1;
    game::police_probe::ObserveCanSpawn(base, managerInterface, argument, ctx.r3.u32);
}

// Reset/enable routines initialize the same cooldown read by CanSpawn. Cap it
// once after those originals, rather than resetting the timer each task tick.
// Cinematic/script and manager block checks still control actual spawning.
PPC_FUNC(sub_820F18F8)
{
    const uint32_t managerInterface = ctx.r3.u32;
    __imp__sub_820F18F8(ctx, base);
    LimitInitialDelay(base, managerInterface);
}

PPC_FUNC(sub_820F21A8)
{
    const uint32_t managerInterface = ctx.r3.u32;
    __imp__sub_820F21A8(ctx, base);
    LimitInitialDelay(base, managerInterface);
}

// Raw attribute readers in the loader and race initialization do not all pass
// through GRaceParameters getters. Cover only the police flag and density;
// heat attributes and their original race/Career behavior pass through.
uint32_t game::race_police::Attribute(const uint8_t* base, uint32_t key, uint32_t index, uint32_t original)
{
    if (!Enabled() || index || (key != 0x3918E889 && key != 0xDBC08D32)) return original;
    const unsigned field = key == 0x3918E889 ? 0 : 4;
    if (field && original)
    {
        uint32_t n;
        std::memcpy(&n, base + original, 4);
        if (int32_t(ByteSwap(n)) >= int32_t(s_density == 3 ? 100u : s_density == 2 ? 66u : 33u)) return original;
    }
    const uint32_t value = AttributeValue(field);
    return value ? value : original;
}

// GRaceParameters: CopsInRace key 0x3918E889; local flag 0x800.
PPC_FUNC(sub_823380E0)
{
    __imp__sub_823380E0(ctx, base);
    if (Enabled())
    {
        Log("CopsInRace", ctx.r3.u32, 1);
        ctx.r3.u64 = 1;
    }
}

// CopDensity key 0xDBC08D32, mapped by the original from percent to 0..3.
PPC_FUNC(sub_8233A738)
{
    __imp__sub_8233A738(ctx, base);
    if (Enabled())
    {
        Log("CopDensity", ctx.r3.u32, std::max(ctx.r3.u32, s_density));
        ctx.r3.u64 = std::max(ctx.r3.u32, s_density);
    }
}
