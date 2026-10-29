// SpeedBreaker. GPL-3.0-or-later (see COPYING).
#pragma once
#include <algorithm>
#include <cmath>

namespace game::speedbreaker
{
    // The USA activation call supplies base mass * 2. Zero clears the game's
    // collision override. Bound the effective mass as well as the multiplier;
    // finite arithmetic alone does not guarantee collision-solver stability.
    inline constexpr double kMaxMultiplier = 1000000.0;
    inline constexpr double kMaxCollisionMass = 1000000000.0;
    inline double ActivationMass(double original, double multiplier)
    {
        if (!std::isfinite(original) || original <= 0 ||
            !std::isfinite(multiplier) || multiplier < 0 || multiplier > kMaxMultiplier)
            return original;
        if (multiplier == 2) return original;
        if (multiplier == 0) return 0;
        return std::min(original * 0.5, kMaxCollisionMass / multiplier) * multiplier;
    }
}
