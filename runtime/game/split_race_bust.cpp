// SpeedBreaker. GPL-3.0-or-later (see COPYING).
#include <stdafx.h>
#include "fair_cops.h"
#include "split_race_bust.h"
#include "split_screen.h"
#include <kernel/function.h>
#include <kernel/memory.h>
#include <region/region.h>

extern "C" PPC_FUNC(__imp__sub_823633D8);
extern "C" PPC_FUNC(__imp__sub_82335150);

namespace
{
    constexpr uint32_t kRaceStatus = 0x82A2CB18;
    constexpr uint32_t kLocalPlayerCount = 0x82C74BDC;
    constexpr unsigned kRacerStride = 0x1A0;
    constexpr unsigned kBusted = 0x1F;
    constexpr unsigned kFinished = 0x21;
    constexpr uint32_t kPerpBusted = 0xCE28AB70;
    constexpr uint32_t kArrestReason = 0x20D60DBF;
    constexpr uint32_t kPostBustReason = 0xFEA34C0A;
    std::atomic<unsigned> waitingPlayers{0};
    std::atomic<uint32_t> pendingPostBustHandle{0};

    bool TwoLocalPlayers(const uint8_t* base)
    {
        return region::Current() == region::Release::Usa && game::split_screen::GameSaysSplit() &&
            PPC_LOAD_U32(kLocalPlayerCount) == 2;
    }

    bool Active(uint8_t* base)
    {
        return TwoLocalPlayers(base) && game::fair_cops::IsCompetitiveRace(base);
    }

    uint32_t LocalRacer(unsigned player, uint32_t race, uint32_t count)
    {
        const uint32_t local = GuestToHostFunction<uint32_t>(0x82366FD8u, player);
        if (!game::split_screen::PlausibleObject(local)) return 0;
        // GRaceStatus looks up the IPlayer's 1-based index in its race roster.
        const unsigned index = GuestToHostFunction<uint32_t>(0x82364C70u, local);
        if (!index || index > count) return 0;
        return race + 0x20 + (index - 1) * kRacerStride;
    }

    bool StillRacing(const uint8_t* base, uint32_t racer)
    {
        for (unsigned flag = 0x1C; flag <= kFinished; ++flag)
            if (base[racer + flag]) return false;
        return true;
    }
}

bool SplitRaceKeepRunningAfterBust(PPCRegister& message)
{
    auto* base = g_memory.base;
    if (!TwoLocalPlayers(base) || PPC_LOAD_U32(message.u32) != kPerpBusted) return false;

    uint32_t handle = PPC_LOAD_U32(message.u32 + 0x10);
    if (!handle) return false;
    const uint32_t reason = PPC_LOAD_U32(message.u32 + 4);
    if (reason == kPostBustReason)
    {
        // Stock sends a separate, delayed message to start the busted flow.
        // Consume it even if the partner finished during that delay.
        if (!pendingPostBustHandle.compare_exchange_strong(handle, 0, std::memory_order_relaxed)) return false;
        fprintf(stderr, "[split-race] suppressed delayed restart menu for handle %08X\n", handle);
        return true;
    }
    if (reason != kArrestReason) return false;
    fprintf(stderr, "[split-race] arrest message for handle %08X\n", handle);
    if (!game::fair_cops::IsCompetitiveRace(base)) return false;

    const uint32_t race = PPC_LOAD_U32(kRaceStatus);
    const unsigned count = PPC_LOAD_U32(race + 0x1A20);
    if (count < 2 || count > 16) return false;
    const uint32_t racers[] = { LocalRacer(0, race, count), LocalRacer(1, race, count) };
    if (!racers[0] || !racers[1] || racers[0] == racers[1]) return false;
    for (unsigned player = 0; player < 2; ++player)
    {
        const uint32_t racer = racers[player];
        if (PPC_LOAD_U32(racer) != handle || !StillRacing(base, racers[1 - player])) continue;

        // Use the stock per-racer bust path for an unfinished driver. The
        // frontend messages are separate from that racer's status and controls.
        const bool finished = base[racer + kFinished] != 0;
        if (!finished && !base[racer + kBusted])
        {
            GuestToHostFunction<void>(0x82333F50u, racer);
            GuestToHostFunction<void>(0x823340B0u, racer);
        }
        const unsigned bit = 1u << player;
        pendingPostBustHandle.store(handle, std::memory_order_relaxed);
        if (!(waitingPlayers.fetch_or(bit, std::memory_order_relaxed) & bit))
            fprintf(stderr, "[split-race] player %u busted %s; player %u still racing\n",
                player + 1, finished ? "after finishing" : "mid-race", 2 - player);
        return true;
    }
    return false;
}

void game::split_race_bust::Update(uint8_t* base)
{
    if (WaitingPlayers() && !Active(base))
    {
        waitingPlayers.store(0, std::memory_order_relaxed);
        fprintf(stderr, "[split-race] cleared the waiting banner at race exit\n");
    }
}

unsigned game::split_race_bust::WaitingPlayers()
{
    return waitingPlayers.load(std::memory_order_relaxed);
}

PPC_FUNC(sub_823633D8)
{
    pendingPostBustHandle.store(0, std::memory_order_relaxed);
    waitingPlayers.store(0, std::memory_order_relaxed);
    __imp__sub_823633D8(ctx, base);
}

PPC_FUNC(sub_82335150)
{
    const uint32_t racer = ctx.r3.u32;
    const bool trace = TwoLocalPlayers(base) && !base[racer + kFinished];
    __imp__sub_82335150(ctx, base);
    if (trace && base[racer + kFinished])
        fprintf(stderr, "[split-race] finish recorded for handle %08X\n", PPC_LOAD_U32(racer));
}
