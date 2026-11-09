// SpeedBreaker. GPL-3.0-or-later (see COPYING).
#include <game/police_population.h>
#include <cassert>

using game::police_population::ScaledBudget;
int main()
{
    for (uint32_t base = 0; base <= 64; ++base)
    {
        assert(ScaledBudget(base, 0, 25) == base);
        for (uint32_t racers = 1; racers <= 16; ++racers)
        {
            assert(ScaledBudget(base, racers, 0) == base);
            const auto scaled = ScaledBudget(base, racers, 25);
            assert(scaled >= base && scaled <= 64);
            if (racers == 4) assert(scaled == std::min(64u, 2 * base));
            assert(scaled >= ScaledBudget(base, racers - 1, 25));
        }
    }
    assert(ScaledBudget(5, 1, 25) == 6);
    assert(ScaledBudget(5, 2, 25) == 7);
    assert(ScaledBudget(5, 6, 25) == 12);
    assert(ScaledBudget(10, 6, 100) == 64);
    assert(ScaledBudget(5, 4, 101) == 5);
    assert(ScaledBudget(5, 17, 25) == 5);
    assert(ScaledBudget(UINT32_MAX, 4, 25) == UINT32_MAX);
}
