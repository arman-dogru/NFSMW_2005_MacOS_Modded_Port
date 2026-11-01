// SpeedBreaker. GPL-3.0-or-later (see COPYING).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace game::police_population
{
    // One racer keeps the base budget. Four or more reach the selected
    // maximum (at most 2x). Round down: small budgets never exceed the scale.
    // The original 64-simable allocation ceiling is not enlarged.
    inline uint32_t ScaledBudget(uint32_t original, uint32_t racers, double maximum)
    {
        if (!original || original > 64 || racers < 2 ||
            !std::isfinite(maximum) || maximum < 1 || maximum > 2)
            return original;
        const double factor = 1 + (maximum - 1) * (std::min(racers, 4u) - 1) / 3;
        return uint32_t(std::min(64.0, std::floor(original * factor + 1e-9)));
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
