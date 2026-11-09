// SpeedBreaker. GPL-3.0-or-later (see COPYING).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace game::police_population
{
    // Add the selected percentage for each starting racer. Round down and
    // keep the original 64-simable allocation ceiling.
    inline uint32_t ScaledBudget(uint32_t original, uint32_t racers, unsigned percentPerRacer)
    {
        if (original > 64 || racers > 16 || percentPerRacer > 100)
            return original;
        return std::min(64u, original * (100 + percentPerRacer * racers) / 100);
    }

    // A manager task gets a scaled ceiling for this call only. The original
    // heat/mode value is restored afterward; disabling cannot compound it.
    class ScopedManagerBudget
    {
        uint8_t* base;
        uint32_t address = 0, original = 0, applied = 0;
    public:
        ScopedManagerBudget(uint8_t* base, uint32_t manager);
        ~ScopedManagerBudget();
        ScopedManagerBudget(const ScopedManagerBudget&) = delete;
        ScopedManagerBudget& operator=(const ScopedManagerBudget&) = delete;
    };

    uint32_t PatrolAttribute(uint8_t* base, uint32_t key, uint32_t index, uint32_t original);
}
