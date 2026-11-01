// SpeedBreaker. GPL-3.0-or-later (see COPYING).
// USA-only guest adapter for the native FairCops assignment policy.
// Xbox evidence, supported layouts, and outstanding tests: docs/NATIVE_MODS.md.
#include <stdafx.h>
#include "fair_cops.h"
#include <kernel/function.h>
#include <region/region.h>
#include <report/report.h>
#include <charconv>

extern "C" PPC_FUNC(__imp__sub_823D4750);
extern "C" PPC_FUNC(__imp__sub_823D5BE0);
extern "C" PPC_FUNC(__imp__sub_820F0270);

namespace game::fair_cops
{
namespace
{
    bool Switch(const char* name, bool fallback = false)
    {
        const char* v = std::getenv(name);
        if (!v) return fallback;
        if (std::strcmp(v, "1") == 0) return true;
        if (std::strcmp(v, "0") == 0) return false;
        fprintf(stderr, "[fair-cops] invalid %s; using %u\n", name, unsigned(fallback));
        return fallback;
    }
    const bool s_enabled = Switch("NFSMW_FAIRCOPS");
    const bool s_skipRoadblocks = Switch("NFSMW_FAIRCOPS_SKIP_ROADBLOCKS", true);
    const bool s_log = Switch("NFSMW_FAIRCOPS_LOG");
    uint32_t Interval(const char* name, uint32_t fallback, uint32_t lo, uint32_t hi, bool allowZero = false)
    {
        const char* v = std::getenv(name);
        if (!v) return fallback;
        uint32_t n = 0;
        auto result = std::from_chars(v, v + std::strlen(v), n);
        if (result.ec != std::errc{} || *result.ptr || ((!allowZero || n) && (n < lo || n > hi)))
        {
            fprintf(stderr, "[fair-cops] invalid %s; using %u\n", name, fallback);
            return fallback;
        }
        return n;
    }
    const Config s_config{
        Interval("NFSMW_FAIRCOPS_REBALANCE", 30, 1, 600),
        Interval("NFSMW_FAIRCOPS_ROTATION", 600, 60, 7200, true)};

    bool Enabled() { return s_enabled && region::Current() == region::Release::Usa; }
    uint32_t Read32(const uint8_t* base, uint32_t addr)
    {
        uint32_t n;
        std::memcpy(&n, base + addr, 4);
        return ByteSwap(n);
    }
    bool Pointer(uint32_t p, uint32_t extent = 0x900)
    {
        return !(p & 3) && p >= 0x10000 && uint64_t(p) + extent < 0x100000000ull;
    }

    // IDs change even if a guest allocation is destroyed and recreated at the
    // same address between two manager updates. No guest object is enlarged.
    struct Lifetime { uint32_t vehicle = 0; Id id = 0; };
    std::array<Lifetime, 512> s_lifetimes{};
    std::mutex s_lifetimeMutex;
    Id s_nextId = 1;

    void Born(uint32_t vehicle)
    {
        std::lock_guard lock(s_lifetimeMutex);
        for (auto& life : s_lifetimes)
            if (life.vehicle == vehicle)
            {
                life.id = s_nextId++;
                return;
            }
        for (auto& life : s_lifetimes)
            if (!life.vehicle)
            {
                life = {vehicle, s_nextId++};
                return;
            }
    }
    void Died(uint32_t vehicle)
    {
        std::lock_guard lock(s_lifetimeMutex);
        for (auto& life : s_lifetimes)
            if (life.vehicle == vehicle)
                life = {};
    }
    Id Identity(uint32_t vehicle)
    {
        std::lock_guard lock(s_lifetimeMutex);
        for (auto life : s_lifetimes)
            if (life.vehicle == vehicle) return life.id;
        return 0; // Unsupported construction path: fail closed.
    }

    // IVehicle slot 34 and slot 31, independently mapped in the USA image.
    bool Live(uint8_t* base, uint32_t vehicle)
    {
        return Pointer(vehicle) && Read32(base, vehicle) == 0x8208AD18 &&
            Read32(base, vehicle + 0xB4) &&
            !GuestToHostFunction<uint32_t>(0x823D2C18u, vehicle);
    }
    bool CopAI(uint8_t* base, uint32_t ai)
    {
        return Pointer(ai) && Read32(base, ai) == 0x82065A78 &&
            Read32(base, ai + 0x800) == 0x82065B58 && base[ai + 0x808] &&
            (!s_skipRoadblocks || Read32(base, ai + 0x8C) != 0x9E55B2E3);
    }
    struct Actor { Id id = 0; uint32_t vehicle = 0, ai = 0; };
    Balancer s_balancer;
    uint32_t s_manager = 0;
    Id s_player = 0;
    uint64_t s_acquires = 0, s_failed = 0;
}

void AfterPursuitUpdate(uint8_t* base, uint32_t manager)
{
    if (!Enabled()) return;
    const auto started = std::chrono::steady_clock::now();
    if (!Pointer(manager) || Read32(base, manager) != 0x82065C9C)
    {
        s_balancer.Reset();
        return;
    }
    // This is the full IVehicle array; manager +0xC4 contains only cops.
    const uint32_t count = Read32(base, 0x82C6F194), data = Read32(base, 0x82C6F18C);
    if (count > 512 || !Pointer(data, count * 4))
    {
        s_balancer.Reset();
        return;
    }
    std::array<Actor, kMaxCops> cops{};
    std::array<Actor, kMaxRacers> racers{};
    size_t nc = 0, nr = 0;
    Id player = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        const uint32_t vehicle = Read32(base, data + i * 4);
        if (!Live(base, vehicle)) continue;
        const Id id = Identity(vehicle);
        if (!id) continue;
        const uint32_t driver = Read32(base, vehicle + 0x9C);
        if (driver == 0 || driver == 3)
        {
            if (nr == racers.size()) { s_balancer.Reset(); return; }
            racers[nr++] = {id, vehicle, 0};
            if (driver == 0) player = id;
        }
        else if (driver == 2)
        {
            const uint32_t ai = Read32(base, vehicle + 0x54);
            if (!CopAI(base, ai)) continue;
            if (nc == cops.size()) { s_balancer.Reset(); return; }
            cops[nc++] = {id, vehicle, ai};
        }
    }
    if (manager != s_manager || player != s_player)
    {
        s_balancer.Reset();
        s_manager = manager;
        s_player = player;
    }
    // Keep rotation's order independent of changes to the game's array order.
    std::sort(racers.begin(), racers.begin() + nr, [](Actor a, Actor b) { return a.id < b.id; });
    std::array<Id, kMaxCops> copIds{};
    std::array<Id, kMaxRacers> racerIds{};
    for (size_t i = 0; i < nc; ++i) copIds[i] = cops[i].id;
    for (size_t i = 0; i < nr; ++i) racerIds[i] = racers[i].id;
    auto assignments = s_balancer.Tick(std::span(copIds).first(nc), std::span(racerIds).first(nr), s_config);
    for (auto a : assignments)
    {
        auto c = std::find_if(cops.begin(), cops.begin() + nc, [a](Actor v) { return v.id == a.cop; });
        auto r = std::find_if(racers.begin(), racers.begin() + nr, [a](Actor v) { return v.id == a.racer; });
        if (c == cops.begin() + nc || r == racers.begin() + nr ||
            Identity(c->vehicle) != c->id || Identity(r->vehicle) != r->id ||
            !CopAI(base, c->ai) || !Live(base, c->vehicle) || !Live(base, r->vehicle)) continue;
        const uint32_t target = Read32(base, c->ai + 0x68);
        if (!Pointer(target, 0x50) || Read32(base, target) != 0x82064E1C) continue;
        // IVehicle::GetSimable returns this -148, not the root object.
        const uint32_t simable = r->vehicle - 0x94;
        if (!base[target + 0x3C] || Read32(base, target + 0x2C) != simable)
        {
            // AITarget::Acquire validates its simable registry and refreshes
            // position/direction. Do not assign its pointer fields directly.
            GuestToHostFunction<void>(0x8210E118u, target, simable);
            ++s_acquires;
            if (!base[target + 0x3C] || Read32(base, target + 0x2C) != simable) ++s_failed;
        }
    }
    static double lastLog = -10, totalUs = 0, maxUs = 0;
    static uint64_t samples = 0;
    const double elapsedUs = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - started).count();
    totalUs += elapsedUs;
    maxUs = std::max(maxUs, elapsedUs);
    ++samples;
    const double now = report::SessionSeconds();
    if (s_log && now - lastLog >= 5)
    {
        lastLog = now;
        fprintf(stderr, "[fair-cops] cops=%zu racers=%zu assignments=%zu acquires=%llu failed=%llu avg_us=%.1f max_us=%.1f\n",
            nc, nr, assignments.size(), (unsigned long long)s_acquires, (unsigned long long)s_failed,
            totalUs / samples, maxUs);
        totalUs = maxUs = 0;
        samples = 0;
        for (size_t i = 0; i < nr; ++i)
        {
            const size_t coverage = std::count_if(assignments.begin(), assignments.end(),
                [&](Assignment a) { return a.racer == racers[i].id; });
            size_t actual = 0;
            for (size_t c = 0; c < nc; ++c)
            {
                const uint32_t target = Read32(base, cops[c].ai + 0x68);
                if (Pointer(target, 0x50) && Read32(base, target) == 0x82064E1C &&
                    base[target + 0x3C] && Read32(base, target + 0x2C) == racers[i].vehicle - 0x94) ++actual;
            }
            fprintf(stderr, "[fair-cops]   racer=%08X assigned=%zu actual=%zu\n", racers[i].vehicle, coverage, actual);
        }
    }
}
}

// PVehicle constructor/destructor: record native lifetime IDs around originals.
PPC_FUNC(sub_820F0270)
{
    const uint32_t manager = ctx.r3.u32;
    __imp__sub_820F0270(ctx, base);
    game::fair_cops::AfterPursuitUpdate(base, manager);
}

PPC_FUNC(sub_823D4750)
{
    const uint32_t owner = ctx.r3.u32;
    __imp__sub_823D4750(ctx, base);
    if (game::fair_cops::Enabled()) game::fair_cops::Born(owner + 0xC0);
}
PPC_FUNC(sub_823D5BE0)
{
    if (game::fair_cops::Enabled()) game::fair_cops::Died(ctx.r3.u32 + 0xC0);
    __imp__sub_823D5BE0(ctx, base);
}
