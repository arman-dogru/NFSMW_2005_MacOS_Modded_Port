// SpeedBreaker. GPL-3.0-or-later (see COPYING).
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <span>

namespace game::fair_cops
{
    using Id = uint64_t;
    inline constexpr size_t kMaxCops = 96, kMaxRacers = 16;
    struct Position { float x, y, z; };
    struct Candidate { Id id; Position position; unsigned coverage; };

    inline Id Dispatch(std::span<const Candidate> racers, const Position* origin)
    {
        const Candidate* best = nullptr;
        double bestDistance = 0;
        for (const auto& racer : racers)
        {
            double distance = 0;
            if (origin)
            {
                const double x = double(racer.position.x) - origin->x;
                const double y = double(racer.position.y) - origin->y;
                const double z = double(racer.position.z) - origin->z;
                distance = x*x + y*y + z*z;
                if (!std::isfinite(distance) || distance > 150.0*150.0) continue;
            }
            if (!best || racer.coverage < best->coverage ||
                (racer.coverage == best->coverage && (distance < bestDistance ||
                    (distance == bestDistance && racer.id < best->id))))
            {
                best = &racer;
                bestDistance = distance;
            }
        }
        return best ? best->id : 0;
    }

    uint32_t SpawnAnchor(uint8_t* base, uint32_t pursuit);
    void Spawned(uint8_t* base, uint32_t cop, uint32_t anchor);
    bool IsCompetitiveRace(uint8_t* base);
    unsigned LocalPursuitPlayers(uint8_t* base);
}
