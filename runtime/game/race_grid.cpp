// SpeedBreaker. GPL-3.0-or-later (see COPYING).
#include <stdafx.h>
#include "split_screen.h"
#include <kernel/function.h>
#include <kernel/memory.h>
#include <region/region.h>

extern "C" PPC_FUNC(__imp__sub_8233B4E0);

namespace
{
    constexpr uint32_t kRaceType = 0x18;
    constexpr uint32_t kRowStride = 24;
    constexpr uint32_t kOpponents = 0x23;
    constexpr uint32_t kLeft = 0x9120409E;
    constexpr uint32_t kRight = 0xB5971BF1;
    constexpr unsigned kMaxRacers = 6;
    const bool kTrace = [] {
        const char* value = std::getenv("NFSMW_RACE_GRID_LOG");
        return value && std::strcmp(value, "1") == 0;
    }();

    uint32_t Read32(uint32_t address)
    {
        uint32_t value;
        std::memcpy(&value, g_memory.base + address, sizeof(value));
        return ByteSwap(value);
    }

    uint8_t* Options(uint32_t database)
    {
        using namespace game::split_screen;
        if (region::Current() != region::Release::Usa || !PlausibleObject(database) ||
            !(Read32(database + kFlagsOffset) & kQuickRaceFlag))
            return nullptr;

        const uint32_t type = Read32(database + kRaceType);
        return type <= 1 ? g_memory.base + database + kRowStride * type : nullptr;
    }
}

void RaceGridOpponentSelection(PPCRegister& row, PPCRegister& selection, PPCRegister& database, PPCRegister& button)
{
    const auto* options = Options(database.u32);
    if (!options || options - g_memory.base != row.u32) return;

    const unsigned players = g_memory.base[database.u32 + game::split_screen::kPlayersOffset];
    if (players != 1 && players != 2) return;
    const unsigned maximum = kMaxRacers - players;
    const unsigned minimum = players == 2 ? 0 : 1;
    const unsigned current = std::clamp(unsigned(options[kOpponents]), minimum, maximum);
    if (button.u32 == kLeft)
        selection.u64 = current == minimum ? maximum : current - 1;
    else if (button.u32 == kRight)
        selection.u64 = current == maximum ? minimum : current + 1;
}

void RaceGridShowAIOptions(PPCRegister& split)
{
    const uint32_t database = Read32(game::split_screen::kDatabasePointer);
    if (auto* options = Options(database))
    {
        // Both modes use the same stock settings row. Keep its count valid
        // when switching between a solo and a split-screen setup.
        const bool twoPlayers = split.u32 != 0;
        options[kOpponents] = std::clamp<unsigned>(options[kOpponents], twoPlayers ? 0 : 1,
            kMaxRacers - (twoPlayers ? 2 : 1));
        split.u64 = 0;
    }
}

void RaceGridSplitOpponentSlots(PPCRegister& database, PPCRegister& count)
{
    if (const auto* options = Options(database.u32))
    {
        count.u64 = std::min<unsigned>(options[kOpponents], kMaxRacers - 2) + 1;
        if (kTrace)
            fprintf(stderr, "[race-grid] split menu type=%u slots=%u\n",
                Read32(database.u32 + kRaceType), count.u32);
    }
}

void RaceGridAllowSplitAI(PPCRegister& allowed, PPCRegister& race)
{
    const uint32_t database = Read32(game::split_screen::kDatabasePointer);
    if (!Options(database) || g_memory.base[database] != 2) return;
    const uint32_t params = Read32(race.u32 + 0x1A30);
    if (!game::split_screen::PlausibleObject(params)) return;
    const unsigned slots = Read32(params + 0x18);
    if (slots > 1 && slots < kMaxRacers) allowed.u64 = 1;
}

PPC_FUNC(sub_8233B4E0)
{
    const uint32_t params = ctx.r3.u32;
    __imp__sub_8233B4E0(ctx, base);
    if (!kTrace || region::Current() != region::Release::Usa) return;
    const uint32_t attributes = Read32(params + 8);
    fprintf(stderr, "[race-grid] custom params=%08x slots=%u list=%u difficulty=%u\n", params,
        Read32(params + 0x18), GuestToHostFunction<uint32_t>(0x82328470u, attributes),
        GuestToHostFunction<uint32_t>(0x8233A6A0u, params));
}
