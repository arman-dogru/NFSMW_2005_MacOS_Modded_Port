// SpeedBreaker. GPL-3.0-or-later (see COPYING).
#pragma once
#include <cstdint>

namespace game::split_race_bust
{
    void Update(uint8_t* base);
    unsigned WaitingPlayers();
}
