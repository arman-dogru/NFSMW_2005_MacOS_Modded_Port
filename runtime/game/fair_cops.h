// SpeedBreaker. GPL-3.0-or-later (see COPYING).
// Native assignment policy for the FairCops port. No guest memory access.
// The adapter passes live, eligible cops/racers, excludes roadblock holders,
// and uses lifetime-aware IDs (a recycled guest pointer alone is not ID).
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

namespace game::fair_cops
{
    // USA guest adapter; a no-op unless NFSMW_FAIRCOPS=1.
    void AfterPursuitUpdate(uint8_t* base, uint32_t manager);

    using Id = uint64_t;
    inline constexpr size_t kMaxCops = 96, kMaxRacers = 16;

    struct Config
    {
        uint32_t rebalanceFrames = 30;
        uint32_t undersupplyRotationFrames = 600; // 0 disables rotation
    };

    struct Assignment
    {
        Id cop = 0, racer = 0;
        bool operator==(const Assignment&) const = default;
    };

    class Balancer
    {
        std::array<Assignment, kMaxCops> assignments{};
        size_t count = 0, rotationOffset = 0;
        uint32_t sinceRebalance = 0, sinceRotation = 0;

        static size_t Find(std::span<const Id> ids, Id id)
        {
            return size_t(std::find(ids.begin(), ids.end(), id) - ids.begin());
        }

        static bool Valid(std::span<const Id> ids)
        {
            for (size_t i = 0; i < ids.size(); ++i)
                if (!ids[i] || Find(ids.first(i), ids[i]) != i)
                    return false;
            return true;
        }

        void Rebuild(std::span<const Id> cops, std::span<const Id> racers, bool rotate)
        {
            const auto old = assignments;
            const size_t oldCount = count;
            std::array<size_t, kMaxRacers> coverage{};
            count = 0;
            if (rotate)
            {
                rotationOffset = (rotationOffset + 1) % racers.size();
                for (size_t i = 0; i < cops.size(); ++i)
                    assignments[count++] = {cops[i], racers[(i + rotationOffset) % racers.size()]};
                return;
            }
            for (Id cop : cops)
            {
                for (size_t i = 0; i < oldCount; ++i)
                {
                    size_t ri = Find(racers, old[i].racer);
                    if (old[i].cop == cop && ri < racers.size())
                    {
                        assignments[count++] = old[i];
                        ++coverage[ri];
                        break;
                    }
                }
            }
            auto end = coverage.begin() + racers.size();
            for (Id cop : cops)
            {
                bool assigned = std::any_of(assignments.begin(), assignments.begin() + count,
                    [cop](Assignment a) { return a.cop == cop; });
                if (assigned)
                    continue;
                size_t ri = size_t(std::min_element(coverage.begin(), end) - coverage.begin());
                assignments[count++] = {cop, racers[ri]};
                ++coverage[ri];
            }
            for (;;)
            {
                size_t lo = size_t(std::min_element(coverage.begin(), end) - coverage.begin());
                size_t hi = size_t(std::max_element(coverage.begin(), end) - coverage.begin());
                if (coverage[hi] <= coverage[lo] + 1)
                    break;
                for (size_t i = 0; i < count; ++i)
                    if (assignments[i].racer == racers[hi])
                    {
                        assignments[i].racer = racers[lo];
                        --coverage[hi];
                        ++coverage[lo];
                        break;
                    }
            }
        }

    public:
        void Reset() { *this = {}; }

        std::span<const Assignment> Current() const { return {assignments.data(), count}; }

        // Once per simulation update, after the game's ordinary pursuit update.
        // The adapter must revalidate every assignment before acquiring a target.
        std::span<const Assignment> Tick(std::span<const Id> cops, std::span<const Id> racers, Config cfg = {})
        {
            if (cops.empty() || racers.size() < 2 || cops.size() > kMaxCops || racers.size() > kMaxRacers ||
                !Valid(cops) || !Valid(racers))
            {
                Reset();
                return Current();
            }
            const uint32_t interval = std::clamp(cfg.rebalanceFrames, 1u, 600u);
            const uint32_t rotation = cfg.undersupplyRotationFrames ?
                std::clamp(cfg.undersupplyRotationFrames, 60u, 7200u) : 0;
            sinceRebalance = std::min(sinceRebalance + 1, 600u);
            sinceRotation = std::min(sinceRotation + 1, 7200u);
            const bool rotate = cops.size() < racers.size() && rotation && sinceRotation >= rotation;
            bool rebuild = count != cops.size() || !count || rotate || sinceRebalance >= interval;
            for (auto a : Current())
                rebuild |= Find(cops, a.cop) == cops.size() || Find(racers, a.racer) == racers.size();
            if (rebuild)
            {
                Rebuild(cops, racers, rotate);
                sinceRebalance = 0;
                if (rotate)
                    sinceRotation = 0;
            }
            return Current();
        }
    };
}
