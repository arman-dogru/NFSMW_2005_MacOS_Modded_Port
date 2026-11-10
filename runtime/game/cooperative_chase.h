// SpeedBreaker. GPL-3.0-or-later (see COPYING).
#pragma once
#include <cstdint>

namespace game::cooperative_chase
{
    bool Active(const uint8_t* base);
    void Update(uint8_t* base);
    unsigned PursuitPlayers();
}
