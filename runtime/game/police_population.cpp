// SpeedBreaker. GPL-3.0-or-later (see COPYING).
#include <stdafx.h>
#include "police_population.h"
#include "fair_cops.h"
#include "frame_time.h"
#include "race_police.h"
#include <kernel/function.h>
#include <region/region.h>
#include <report/report.h>
#include <user/settings.h>

extern "C" PPC_FUNC(__imp__sub_820ECFB8);
extern "C" PPC_FUNC(__imp__sub_820ECA58);
extern "C" PPC_FUNC(__imp__sub_8211A428);
extern "C" PPC_FUNC(__imp__sub_8211B9E8);
extern "C" PPC_FUNC(__imp__sub_82118F80);

namespace game::police_population
{
namespace
{
    uint32_t Read32(const uint8_t* base, uint32_t address)
    {
        uint32_t value;
        std::memcpy(&value, base + address, 4);
        return ByteSwap(value);
    }
    void Write32(uint8_t* base, uint32_t address, uint32_t value)
    {
        value = ByteSwap(value);
        std::memcpy(base + address, &value, 4);
    }
    float ReadFloat(const uint8_t* base, uint32_t address)
    {
        const uint32_t bits = Read32(base, address);
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }
    bool Pointer(uint32_t p, uint32_t extent = 0x100)
    {
        return !(p & 3) && p >= 0x10000 && uint64_t(p) + extent < 0x100000000ull;
    }
    const bool s_modded = [] {
        const char* fair = std::getenv("NFSMW_FAIRCOPS");
        return fair && std::strcmp(fair, "1") == 0;
    }();
    const bool s_log = [] {
        const char* v = std::getenv("NFSMW_POLICE_SPAWN_LOG");
        return v && std::strcmp(v, "1") == 0;
    }();
    bool ActiveRace(const uint8_t* base)
    {
        if ((!s_modded && !race_police::Enabled()) || region::Current() != region::Release::Usa) return false;
        const uint32_t race = Read32(base, 0x82A2CB18);
        return Pointer(race, 0x1A34) && Read32(base, race + 0x1A28) == 1 &&
            Read32(base, race + 0x1A30);
    }
    bool Live(const uint8_t* base, uint32_t vehicle)
    {
        return Pointer(vehicle) && Read32(base, vehicle) == 0x8208AD18 &&
            Read32(base, vehicle + 0xB4) &&
            !GuestToHostFunction<uint32_t>(0x823D2C18u, vehicle);
    }
    struct Racer { uint32_t vehicle, simable; float x, y, z; };
    struct Snapshot
    {
        std::array<Racer, 16> racers{};
        uint32_t count = 0;
    };
    const Snapshot& Racers(uint8_t* base)
    {
        // Manager work runs on its guest task thread. Avoid repeated guest
        // calls for each cop in the same frame, without sharing host pointers.
        static thread_local uint64_t lastFrame = UINT64_MAX;
        static thread_local Snapshot snapshot;
        const uint64_t frame = game::GetFrameTimeStats().frames;
        if (!ActiveRace(base)) { snapshot.count = 0; lastFrame = UINT64_MAX; return snapshot; }
        if (frame == lastFrame) return snapshot;
        lastFrame = frame;
        snapshot.count = 0;
        const uint32_t count = Read32(base, 0x82C6F194), data = Read32(base, 0x82C6F18C);
        if (count > 512 || !Pointer(data, count * 4)) return snapshot;
        bool human = false;
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t vehicle = Read32(base, data + 4 * i);
            if (!Live(base, vehicle)) continue;
            const uint32_t driver = Read32(base, vehicle + 0x9C);
            if (driver != 0 && driver != 3) continue;
            if (snapshot.count == snapshot.racers.size()) { snapshot.count = 0; return snapshot; }
            const uint32_t simable = vehicle - 0x94, vt = Read32(base, simable);
            if (!Pointer(vt, 0x64)) continue;
            const uint32_t position = GuestToHostFunction<uint32_t>(Read32(base, vt + 0x60), simable);
            if (!Pointer(position, 12)) continue;
            Racer racer{vehicle, simable, ReadFloat(base, position), ReadFloat(base, position + 4), ReadFloat(base, position + 8)};
            if (!std::isfinite(racer.x) || !std::isfinite(racer.y) || !std::isfinite(racer.z)) continue;
            snapshot.racers[snapshot.count++] = racer;
            human |= driver == 0;
        }
        if (!human) snapshot.count = 0;
        return snapshot;
    }
    uint32_t StartingRacers(const uint8_t* base)
    {
        // GRaceStatus counts the starting records, including both local
        // players. Active vehicle counts can drop during a race.
        const uint32_t race = Read32(base, 0x82A2CB18);
        return race ? Read32(base, race + 0x1A20) : 0;
    }
    uint32_t Scale(uint8_t* base, uint32_t original)
    {
        return ScaledBudget(original, StartingRacers(base),
            settings::GetInt(settings::Id::PoliceBudgetPerRacer));
    }
    thread_local uint32_t s_spawnTarget = 0;
    thread_local bool s_spawnTargetUsed = false;
    uint32_t TemporaryTarget(uint8_t* base, uint32_t simable)
    {
        static thread_local uint32_t address = [] {
            auto* memory = g_userHeap.Alloc(0x50);
            return memory ? g_memory.MapVirtual(memory) : 0u;
        }();
        if (!address) return 0;
        // An unregistered scratch AITarget, used only by the original spawn
        // finder. Acquire/Reset/Update access +0x18..+0x4C without calling
        // this object's virtual methods. Never link it into the world's
        // target list or expose it as a formal pursuit object.
        std::memset(base + address, 0, 0x50);
        Write32(base, address, 0x82064E1C);
        GuestToHostFunction<void>(0x8210E118u, address, simable);
        return base[address + 0x3C] && Read32(base, address + 0x2C) == simable ? address : 0;
    }
}

ScopedManagerBudget::ScopedManagerBudget(uint8_t* memory, uint32_t manager) : base(memory)
{
    if (!ActiveRace(base) || !Pointer(manager, 0x9C) || Read32(base, manager) != 0x82065C9C) return;
    address = manager + 0x98;
    original = Read32(base, address);
    applied = Scale(base, original);
    if (applied != original)
    {
        Write32(base, address, applied);
        static uint32_t lastManager = 0, lastBudget = 0;
        if (s_log && (lastManager != manager || lastBudget != applied))
            fprintf(stderr, "[police-budget] manager=%08X ceiling=%u -> %u racers=%u\n",
                manager, original, applied, StartingRacers(base));
        lastManager = manager;
        lastBudget = applied;
    }
}
ScopedManagerBudget::~ScopedManagerBudget()
{
    if (address && applied != original && Read32(base, address) == applied)
        Write32(base, address, original);
}

uint32_t PatrolAttribute(uint8_t* base, uint32_t key, uint32_t index, uint32_t original)
{
    if (key != 0x24F7A1BC || index || !original || !ActiveRace(base)) return original;
    const uint32_t count = Read32(base, original), scaled = Scale(base, count);
    if (scaled == count) return original;
    static const uint32_t values = [] {
        auto* memory = static_cast<uint8_t*>(g_userHeap.Alloc(65 * 4));
        if (!memory) return 0u;
        for (uint32_t i = 0; i <= 64; ++i) Write32(memory, i * 4, i);
        return g_memory.MapVirtual(memory);
    }();
    return values ? values + scaled * 4 : original;
}
} // namespace game::police_population

// A single new pursuit car: choose its anchor once for all original placement
// retries. Roadblock/formation placement functions are not wrapped.
PPC_FUNC(sub_820ECFB8)
{
    using namespace game::police_population;
    const uint32_t pursuit = ctx.r4.u32, cop = ctx.r5.u32;
    const bool enabled = ActiveRace(base) && settings::GetBool(settings::Id::RacerPoliceSpawns) &&
        Pointer(pursuit, 0x78) && Read32(base, pursuit) == 0x82064CD8;
    const uint32_t racer = enabled ? game::fair_cops::SpawnAnchor(base, pursuit) : 0;
    const uint32_t saved = s_spawnTarget;
    const bool savedUsed = s_spawnTargetUsed;
    s_spawnTarget = racer ? TemporaryTarget(base, racer - 0x94) : 0;
    s_spawnTargetUsed = false;
    __imp__sub_820ECFB8(ctx, base);
    const bool used = s_spawnTargetUsed && (ctx.r3.u32 & 0xFF);
    s_spawnTarget = saved;
    s_spawnTargetUsed = savedUsed;
    if (ctx.r3.u32 & 0xFF) game::fair_cops::Spawned(base, cop, used ? racer : 0);
    if (used)
    {
        static unsigned lines = 0;
        if (s_log && lines++ < 96)
            fprintf(stderr, "[police-spawn] anchor racer=%08X cop=%08X\n", racer, cop);
    }
}

// Retry only the placement search. The car has not been activated or
// attached yet, so a failed alternate anchor can use the original route.
PPC_FUNC(sub_820ECA58)
{
    using namespace game::police_population;
    if (!s_spawnTarget || ctx.lr != 0x820ED610)
    {
        __imp__sub_820ECA58(ctx, base);
        return;
    }
    const auto entry = ctx;
    __imp__sub_820ECA58(ctx, base);
    if (ctx.r3.u32 & 0xFF) return;
    const auto anchor = s_spawnTarget;
    s_spawnTarget = 0;
    s_spawnTargetUsed = false;
    ctx = entry;
    __imp__sub_820ECA58(ctx, base);
    s_spawnTarget = anchor;
}

// IPursuit::GetTarget returns +0x74. Only the spawn finder's first call may
// see the scratch target. Formal pursuit membership/target is never changed.
PPC_FUNC(sub_8211A428)
{
    const uint32_t caller = ctx.lr;
    __imp__sub_8211A428(ctx, base);
    if (caller == 0x820ECA94 && game::police_population::s_spawnTarget)
    {
        ctx.r3.u64 = game::police_population::s_spawnTarget;
        game::police_population::s_spawnTargetUsed = true;
    }
}

// The manager's distance/visibility test is the only caller of this routine.
// A cop close to another live racer should not be out of range just because
// the player drove away. Other heat/pursuit/vehicle cleanup remains original.
PPC_FUNC(sub_8211B9E8)
{
    using namespace game::police_population;
    const uint32_t caller = ctx.lr, sphere = ctx.r3.u32, point = ctx.r4.u32;
    __imp__sub_8211B9E8(ctx, base);
    if (!ctx.r3.u32 || caller != 0x820EEBD4 || !ActiveRace(base) ||
        !settings::GetBool(settings::Id::KeepPoliceNearRacers) ||
        !Pointer(sphere, 12) || Read32(base, sphere) != 0x820651D8 || !Pointer(point, 12)) return;
    const double radius = ReadFloat(base, sphere + 8), x = ReadFloat(base, point), z = ReadFloat(base, point + 8);
    if (!std::isfinite(radius) || radius <= 0 || radius > 10000 || !std::isfinite(x) || !std::isfinite(z)) return;
    const auto& racers = Racers(base);
    if (racers.count < 2) return;
    for (uint32_t i = 0; i < racers.count; ++i)
    {
        const double dx = x - racers.racers[i].x, dz = z - racers.racers[i].z;
        if (dx * dx + dz * dz > radius * radius) continue;
        ctx.r3.u64 = 0;
        static unsigned lines = 0;
        if (s_log && lines++ < 64)
            fprintf(stderr, "[police-retain] racer=%08X distance=%.1f radius=%.1f\n",
                racers.racers[i].vehicle, std::hypot(dx, dz), radius);
        break;
    }
}

// The original fills caller-owned 0x18-byte car-table rows. Scale each ground
// model's requested count (+0x10), not VLT/shared fallback data. The original
// helicopter row (hash at +8) is excluded, as in the original count summation.
PPC_FUNC(sub_82118F80)
{
    using namespace game::police_population;
    const uint32_t rows = ctx.r4.u32, countAddress = ctx.r5.u32;
    __imp__sub_82118F80(ctx, base);
    if (!ActiveRace(base) || !Pointer(countAddress, 4)) return;
    const uint32_t count = Read32(base, countAddress);
    if (count > 9 || !Pointer(rows, count * 0x18)) return;
    const uint32_t helicopter = Read32(base, 0x82A2DF44);
    for (uint32_t i = 0; i < count; ++i)
    {
        const uint32_t row = rows + i * 0x18;
        if (Read32(base, row + 8) == helicopter) continue;
        const uint32_t original = Read32(base, row + 0x10), scaled = Scale(base, original);
        if (scaled == original) continue;
        Write32(base, row + 0x10, scaled);
        static unsigned lines = 0;
        if (s_log && lines++ < 24)
            fprintf(stderr, "[police-budget] racers=%u model=%08X cars=%u -> %u\n",
                StartingRacers(base), Read32(base, row + 8), original, scaled);
    }
}
