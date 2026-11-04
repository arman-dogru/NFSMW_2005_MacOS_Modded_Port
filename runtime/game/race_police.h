// SpeedBreaker. GPL-3.0-or-later (see COPYING).
#pragma once
#include <cstdint>
namespace game::race_police
{
    bool CanSpawn(const uint8_t* base, uint32_t managerInterface, uint32_t argument, bool original);
    // Composed with the existing language attribute hook. Returns original
    // unchanged outside the two supported USA police attributes. Heat stays stock.
    uint32_t Attribute(const uint8_t* base, uint32_t key, uint32_t index, uint32_t original);
}
