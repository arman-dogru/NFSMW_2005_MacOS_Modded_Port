// SpeedBreaker. GPL-3.0-or-later (see COPYING).
#include <stdafx.h>
#include "auto_drive.h"
#include "cooperative_chase.h"
#include "fair_cops.h"
#include "race_police.h"
#include "split_race_bust.h"
#include <kernel/function.h>
#include <region/region.h>
#include <bit>

extern "C" PPC_FUNC(__imp__sub_823AFE20);
extern "C" PPC_FUNC(__imp__sub_82201628);
extern "C" PPC_FUNC(__imp__sub_82201720);
extern "C" PPC_FUNC(__imp__sub_82201818);
extern "C" PPC_FUNC(__imp__sub_82201B50);
extern "C" PPC_FUNC(__imp__sub_82201258);
extern "C" PPC_FUNC(__imp__sub_82203690);

namespace
{
    const bool enabled = [] {
        const char* value = std::getenv("NFSMW_RACE_EA_TRAX_ONLY");
        return value && std::strcmp(value, "1") == 0;
    }();
    const bool logMusic = [] {
        const char* value = std::getenv("NFSMW_RACE_MUSIC_LOG");
        return value && std::strcmp(value, "1") == 0;
    }();
    std::atomic<bool> raceMusic{false};

    uint32_t Read32(const uint8_t* base, uint32_t address)
    {
        uint32_t value;
        std::memcpy(&value, base + address, sizeof(value));
        return ByteSwap(value);
    }
    bool Pointer(uint32_t p, uint32_t extent)
    {
        return !(p & 3) && p >= 0x10000 && uint64_t(p) + extent < 0x100000000ull;
    }
    bool InRace(uint8_t* base)
    {
        if (!game::fair_cops::IsCompetitiveRace(base)) return false;
        // The stock cop watch loop uses this same local vehicle list.
        const uint32_t data = Read32(base, 0x82C6F1EC);
        if (Read32(base, 0x82C6F1F4) != 1 || !Pointer(data, 4)) return false;
        const uint32_t vehicle = Read32(base, data);
        if (!Pointer(vehicle, 0xB8) || Read32(base, vehicle) != 0x8208AD18 ||
            Read32(base, vehicle + 0x9C) != 0) return false;
        const uint32_t race = Read32(base, 0x82A2CB18);
        const uint32_t racer = GuestToHostFunction<uint32_t>(0x8232F7F0u, race, vehicle - 0x94);
        if (!Pointer(racer, 0x22)) return false;
        // Packed GRacerInfo terminal flags: KO, totalled, engine, busted,
        // challenge complete, finished. The stock FinishRace sets +0x21.
        for (uint32_t flag = 0x1C; flag <= 0x21; ++flag)
            if (base[racer + flag]) return false;
        return true;
    }
    bool RacePathfinder(const uint8_t* base, uint32_t object)
    {
        return raceMusic.load(std::memory_order_relaxed) && Pointer(object, 0x170) &&
            Read32(base, object) == 0x82072ED0 && Read32(base, object + 0x150) == 1 &&
            !(Read32(base, object + 0x114) & 0x800) && !base[object + 0x118];
    }
    // This belongs to the Pathfinder update thread. Remember a silent stop
    // so an active chase can start its score again when the override ends.
    thread_local uint32_t silentObject = 0;
    bool UpdateScore(uint8_t* base, uint32_t object)
    {
        if (!RacePathfinder(base, object)) return false;
        const uint32_t type = Read32(base, object + 0x148);
        const uint32_t project = Read32(base, object + 0x13C);
        if (type > 2 || project > 1) return false;
        const uint32_t sound = Read32(base, 0x82A2BFFC);
        if (!Pointer(sound, 0x74) || Read32(base, sound + 0x70) == 1) return false;
        const uint32_t settings = Read32(base, sound + 0x24);
        if (!Pointer(settings, 0x30)) return false;
        const float master = std::bit_cast<float>(Read32(base, settings));
        const float volume = std::bit_cast<float>(Read32(base, settings + 0xC));
        const bool licensed = master * volume > 0.0f && Read32(base, settings + 0x2C) == 1 &&
            Read32(base, 0x82A327C8);
        const uint32_t track = Read32(base, object + 76 + 116 * project);
        const uint32_t state = Read32(base, 0x82A2C214 + 4 * project);
        if (!track || !Pointer(state, 0x3C)) return false;
        const bool stopped = Read32(base, state + 0x38) == 1;
        if (!licensed)
        {
            if (silentObject != object || !stopped)
                GuestToHostFunction<void>(0x825C4B30u, track);
            silentObject = object;
            return true;
        }
        if (type != 0 || stopped)
        {
            if (!stopped) GuestToHostFunction<void>(0x825C4B30u, track);
            GuestToHostFunction<void>(0x82200ED8u, object, 0u);
            if (logMusic && type != 0) fprintf(stderr, "[race-music] selected EA Trax\n");
        }
        silentObject = 0;
        return true;
    }
}

PPC_FUNC(sub_823AFE20)
{
    game::race_police::UpdateStartingHeat(base);
    game::auto_drive::Update(base);
    game::cooperative_chase::Update(base);
    game::split_race_bust::Update(base);
    if (enabled && region::Current() == region::Release::Usa)
    {
        const bool active = InRace(base);
        const bool previous = raceMusic.exchange(active, std::memory_order_relaxed);
        if (logMusic && active != previous)
            fprintf(stderr, "[race-music] race override %s\n", active ? "on" : "off");
    }
    __imp__sub_823AFE20(ctx, base);
}

PPC_FUNC(sub_82201628)
{
    const bool racing = RacePathfinder(base, ctx.r3.u32);
    __imp__sub_82201628(ctx, base);
    if (racing && ctx.r3.u32 == 1) ctx.r3.u64 = 0;
}

PPC_FUNC(sub_82201720)
{
    if (RacePathfinder(base, ctx.r3.u32)) ctx.r3.u64 = 0;
    else __imp__sub_82201720(ctx, base);
}

PPC_FUNC(sub_82201818)
{
    // Ambient and dynamic-off arms of the normal in-game scheduler.
    if ((ctx.lr == 0x82201FF0 || ctx.lr == 0x82202178) && UpdateScore(base, ctx.r3.u32))
        ctx.r3.u64 = 1;
    else __imp__sub_82201818(ctx, base);
}

PPC_FUNC(sub_82201B50)
{
    const uint32_t object = ctx.r3.u32;
    const bool scheduler = ctx.lr == 0x82202104 || ctx.lr == 0x8220218C || ctx.lr == 0x822021F8;
    if (scheduler && UpdateScore(base, object))
    {
        ctx.r3.u64 = 1;
        return;
    }
    if (scheduler && silentObject == object && !raceMusic.load(std::memory_order_relaxed))
    {
        silentObject = 0;
        if (GuestToHostFunction<uint32_t>(0x82201720u, object) & 0xFF)
        {
            ctx.r3.u64 = 1;
            return;
        }
    }
    __imp__sub_82201B50(ctx, base);
}

PPC_FUNC(sub_82201258)
{
    if (ctx.r4.u32 == 0x016E7282 && RacePathfinder(base, ctx.r3.u32)) return;
    __imp__sub_82201258(ctx, base);
}

PPC_FUNC(sub_82203690)
{
    const uint32_t object = ctx.r3.u32, message = ctx.r4.u32;
    if (RacePathfinder(base, object) && Read32(base, object + 0x148) == 1 && Pointer(message, 24))
    {
        const uint32_t event = Read32(base, message + 20);
        if (event != 9 && event != 16) return;
    }
    __imp__sub_82203690(ctx, base);
}
