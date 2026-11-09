// SpeedBreaker. GPL-3.0-or-later (see COPYING).
#include <game/fair_cops.h>
#include <cassert>
#include <limits>

using namespace game::fair_cops;

int main()
{
    const Position origin{0, 0, 0};
    std::array<Candidate, 4> racers{{
        {10, {8, 0, 0}, 1},
        {20, {160, 0, 0}, 0},
        {30, {150, 0, 0}, 0},
        {40, {10, 0, 0}, 0},
    }};
    assert(Dispatch(racers, &origin) == 40);
    assert(Dispatch(racers, nullptr) == 20);
    racers[3].coverage = 1;
    assert(Dispatch(racers, &origin) == 30);
    racers[2].position.y = 1;
    assert(Dispatch(racers, &origin) == 10);
    racers[0].position.x = std::numeric_limits<float>::quiet_NaN();
    assert(Dispatch(racers, &origin) == 40);
    assert(Dispatch({}, &origin) == 0);
}
