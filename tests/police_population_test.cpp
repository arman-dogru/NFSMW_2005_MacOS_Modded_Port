// SpeedBreaker. GPL-3.0-or-later (see COPYING).
#include <game/police_population.h>
#include <cassert>
#include <limits>

using game::police_population::ScaledBudget;
int main()
{
    for (uint32_t base = 0; base <= 64; ++base)
    {
        assert(ScaledBudget(base, 0, 2) == base);
        assert(ScaledBudget(base, 1, 2) == base);
        for (uint32_t racers = 1; racers <= 16; ++racers)
        {
            assert(ScaledBudget(base, racers, 1) == base);
            const auto scaled = ScaledBudget(base, racers, 2);
            assert(scaled >= base && scaled <= base * 2 && scaled <= 64);
            if (racers >= 4) assert(scaled == std::min(64u, 2 * base));
            if (racers > 1) assert(scaled >= ScaledBudget(base, racers - 1, 2));
        }
    }
    assert(ScaledBudget(5, 2, 2) == 6);
    assert(ScaledBudget(5, 3, 2) == 8);
    assert(ScaledBudget(5, 4, 2) == 10);
    assert(ScaledBudget(5, 4, 1.5) == 7);
    assert(ScaledBudget(5, 4, 0) == 5);
    assert(ScaledBudget(5, 4, 3) == 5);
    assert(ScaledBudget(5, 4, std::numeric_limits<double>::infinity()) == 5);
    assert(ScaledBudget(5, 4, std::numeric_limits<double>::quiet_NaN()) == 5);
    assert(ScaledBudget(UINT32_MAX, 4, 2) == UINT32_MAX); // unknown budget fails closed
}
