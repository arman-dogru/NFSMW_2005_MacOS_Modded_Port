// SpeedBreaker. GPL-3.0-or-later (see COPYING).
#include <game/fair_cops.h>
#include <cassert>
#include <vector>

using namespace game::fair_cops;

static void Check(std::span<const Assignment> assignments, std::span<const Id> cops, std::span<const Id> racers)
{
    assert(assignments.size() == cops.size());
    std::vector<size_t> coverage(racers.size());
    for (Id cop : cops)
    {
        assert(std::count_if(assignments.begin(), assignments.end(),
            [cop](auto a) { return a.cop == cop; }) == 1);
    }
    for (auto a : assignments)
    {
        auto p = std::find(racers.begin(), racers.end(), a.racer);
        assert(p != racers.end());
        ++coverage[size_t(p - racers.begin())];
    }
    assert(*std::max_element(coverage.begin(), coverage.end()) - *std::min_element(coverage.begin(), coverage.end()) <= 1);
}

int main()
{
    // All supported population sizes, stable across rebalances and list reorder.
    for (size_t nc = 1; nc <= kMaxCops; ++nc)
        for (size_t nr = 2; nr <= kMaxRacers; ++nr)
        {
            Balancer b;
            std::vector<Id> cops(nc), racers(nr);
            for (size_t i = 0; i < nc; ++i) cops[i] = 100 + i;
            for (size_t i = 0; i < nr; ++i) racers[i] = 1000 + i;
            auto a = b.Tick(cops, racers, {30, 0});
            Check(a, cops, racers);
            std::vector<Assignment> initial(a.begin(), a.end());
            std::reverse(cops.begin(), cops.end());
            std::reverse(racers.begin(), racers.end());
            for (int t = 0; t < 65; ++t) Check(b.Tick(cops, racers, {30, 0}), cops, racers);
            for (auto old : initial)
                assert(std::find(b.Current().begin(), b.Current().end(), old) != b.Current().end());
            // Churn and recycled-address IDs trigger an immediate rebuild.
            cops[0] += (1ull << 32);
            racers[0] += (1ull << 32);
            Check(b.Tick(cops, racers), cops, racers);
        }
    Balancer b;
    const std::array<Id, 1> cop{100};
    const std::array<Id, 4> racers{10,20,30,40};
    std::array<bool, 4> seen{};
    Id previous = 0;
    for (int tick = 1; tick <= 241; ++tick)
    {
        auto a = b.Tick(cop, racers, {30,60});
        Id target = a[0].racer;
        if (tick > 1 && tick % 60 != 0) assert(target == previous);
        seen[size_t(std::find(racers.begin(), racers.end(), target) - racers.begin())] = true;
        previous = target;
    }
    assert(std::all_of(seen.begin(), seen.end(), [](bool v) { return v; }));
    assert(b.Tick(cop, std::span(racers).first(1)).empty());
    assert(b.Tick(cop, racers)[0].racer == racers[0]); // session reset
    const std::array<Id, 2> duplicate{100,100}, zero{0,100};
    assert(b.Tick(duplicate, racers).empty());
    assert(b.Tick(zero, racers).empty());
    std::vector<Id> overflow(kMaxCops + 1, 100);
    assert(b.Tick(overflow, racers).empty());
}
