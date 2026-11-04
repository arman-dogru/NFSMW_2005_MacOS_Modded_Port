// SpeedBreaker. GPL-3.0-or-later (see COPYING).
#pragma once
#include <cstdint>

namespace game::boss_rewards
{
    constexpr uint32_t kScreenVtable = 0x820A5C74;
    constexpr uint32_t kVisibleCount = 0x2C;
    constexpr uint32_t kMaximumCards = 6;

    constexpr bool LimitCaller(uint32_t caller)
    {
        return caller == 0x82586868 || caller == 0x82586998 ||
            caller == 0x825870C0 || caller == 0x82587158;
    }

    // These four original callers compare against 2 or display 2 - count.
    // Supply a signed, shifted count only to them: both operations then use
    // the number of available cards. The stored selected flags stay real.
    constexpr int32_t CountForOriginalLimit(uint32_t caller, uint32_t selected,
        uint32_t available, bool enabled)
    {
        if (!enabled || !LimitCaller(caller) || !available ||
            available > kMaximumCards || selected > available)
            return int32_t(selected);
        return int32_t(selected) - (int32_t(available) - 2);
    }
}
