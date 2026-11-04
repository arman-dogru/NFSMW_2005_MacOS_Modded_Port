// SpeedBreaker. GPL-3.0-or-later (see COPYING).
#include <game/boss_rewards.h>
#include <array>
#include <cassert>
#include <cstdint>

using namespace game::boss_rewards;

int main()
{
    constexpr std::array<uint32_t, 4> callers{0x82586868, 0x82586998, 0x825870C0, 0x82587158};
    for (uint32_t available = 1; available <= 6; ++available)
        for (uint32_t count = 0; count <= available; ++count)
            for (auto caller : callers)
            {
                const int32_t view = CountForOriginalLimit(caller, count, available, true);
                assert((view < 2) == (count < available)); // selection still allowed
                assert((view >= 2) == (count == available)); // continue after final card
                assert(2 - view == int32_t(available - count)); // both remaining labels
                assert(CountForOriginalLimit(caller, count, available, false) == int32_t(count));
                assert(CountForOriginalLimit(0, count, available, true) == int32_t(count));
            }
    assert(CountForOriginalLimit(callers[0], 2, 7, true) == 2);
    assert(CountForOriginalLimit(callers[0], 2, 1, true) == 2);
    assert(CountForOriginalLimit(callers[0], 0, 0, true) == 0);
}
