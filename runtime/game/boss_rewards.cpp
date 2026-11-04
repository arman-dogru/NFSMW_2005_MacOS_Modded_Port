// SpeedBreaker. GPL-3.0-or-later (see COPYING).
// USA boss marker selection: docs/BOSS_REWARDS.md.
#include <stdafx.h>
#include <region/region.h>
#include <user/settings.h>
#include "boss_rewards.h"

extern "C" PPC_FUNC(__imp__sub_82586E28);

namespace
{
    uint32_t Read32(const uint8_t* base, uint32_t address)
    {
        uint32_t value;
        std::memcpy(&value, base + address, sizeof(value));
        return ByteSwap(value);
    }
}

// FEMarkerSelection::GetNumSelected. Preserve the original reward delivery,
// duplicate-card guard and continuation; adjust its two limits and two labels.
PPC_FUNC(sub_82586E28)
{
    using namespace game::boss_rewards;
    const uint32_t screen = ctx.r3.u32, caller = uint32_t(ctx.lr);
    const bool enabled = region::Current() == region::Release::Usa &&
        settings::GetBool(settings::Id::AllBossRewards) && LimitCaller(caller) &&
        !(screen & 3) && screen >= 0x10000 && uint64_t(screen) + 0xBC < 0x100000000ull &&
        Read32(base, screen) == kScreenVtable;
    __imp__sub_82586E28(ctx, base);
    if (enabled)
        ctx.r3.s64 = CountForOriginalLimit(caller, ctx.r3.u32,
            Read32(base, screen + kVisibleCount), true);
}
