// SpeedBreaker. GPL-3.0-or-later (see COPYING).
// USA-only guest adapter for the native FairCops assignment policy.
// Xbox evidence, supported layouts, and outstanding tests: docs/NATIVE_MODS.md.
#include <stdafx.h>
#include "fair_cops.h"
#include "split_screen.h"
#include <kernel/function.h>
#include <region/region.h>
#include <report/report.h>
#include <kernel/heap.h>
#include <bit>
#include <limits>

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
            (!s_skipRoadblocks || (!Read32(base, ai + 0x88) &&
                Read32(base, ai + 0x8C) != 0x9E55B2E3));
    }
    struct Actor
    {
        Id id = 0;
        uint32_t vehicle = 0, ai = 0;
        Position position{};
        bool local = false;
    };
    struct Assignment
    {
        Id cop = 0, racer = 0;
        uint32_t ai = 0, pursuit = 0;
        float lastProbe = -1, seenPlayer = -1;
        Id probedPlayer = 0;
        uint8_t playerHidden = 0;
        bool reinforcement = false;
    };
    struct World
    {
        std::array<Actor, kMaxRacers> racers{};
        std::array<Assignment, kMaxCops> assignments{};
        size_t count = 0, probeCursor = 0;
        uint32_t manager = 0, playerPursuit = 0, pendingContact = 0, splitRace = 0;
        Id player = 0;
        float time = 0;
    };
    thread_local World world;
    thread_local uint32_t sightTarget = 0;

    void LogSplitAssignment(uint32_t cop, const Actor& target, uint32_t pursuit)
    {
        if (s_log && world.splitRace)
            fprintf(stderr, "[fair-cops] split cop=%08X target=%08X local=%u pursuit=%08X\n",
                cop, target.vehicle, unsigned(target.local), pursuit);
    }

    uint32_t Simable(uint32_t vehicle) { return vehicle - 0x94; }
    float ReadFloat(const uint8_t* base, uint32_t address)
    {
        return std::bit_cast<float>(Read32(base, address));
    }
    void WriteFloat(uint8_t* base, uint32_t address, float value)
    {
        const uint32_t bits = ByteSwap(std::bit_cast<uint32_t>(value));
        std::memcpy(base + address, &bits, sizeof(bits));
    }
    bool ReadPosition(uint8_t* base, uint32_t vehicle, Position& position)
    {
        const uint32_t simable = Simable(vehicle), vt = Read32(base, simable);
        if (!Pointer(vt, 0x64)) return false;
        const uint32_t address = GuestToHostFunction<uint32_t>(Read32(base, vt + 0x60), simable);
        if (!Pointer(address, 12)) return false;
        position = {ReadFloat(base, address), ReadFloat(base, address + 4), ReadFloat(base, address + 8)};
        return std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z);
    }
    uint32_t TargetSimable(const uint8_t* base, uint32_t target)
    {
        return Pointer(target, 0x50) && Read32(base, target) == 0x82064E1C && base[target + 0x3C]
            ? Read32(base, target + 0x2C) : 0;
    }
    bool Pursuit(const uint8_t* base, uint32_t pursuit)
    {
        return Pointer(pursuit) && Read32(base, pursuit) == 0x82064CD8;
    }
    uint32_t FormalTarget(const uint8_t* base, uint32_t pursuit)
    {
        return Pursuit(base, pursuit) ? TargetSimable(base, Read32(base, pursuit + 0x74)) : 0;
    }
    const Actor* Racer(Id id)
    {
        for (const auto& racer : std::span(world.racers).first(world.count))
            if (racer.id == id) return &racer;
        return nullptr;
    }
    const Actor* RacerBySimable(uint32_t simable)
    {
        for (const auto& racer : std::span(world.racers).first(world.count))
            if (Simable(racer.vehicle) == simable) return &racer;
        return nullptr;
    }
    uint32_t OwnPursuit(const uint8_t* base, const Actor& player)
    {
        if (!Pointer(player.ai)) return 0;
        const uint32_t vt = Read32(base, player.ai);
        if (!Pointer(vt, 0xA4) || Read32(base, vt + 0xA0) != 0x820E90F8) return 0;
        const uint32_t pursuit = Read32(base, player.ai + 0x84);
        return FormalTarget(base, pursuit) == Simable(player.vehicle) ? pursuit : 0;
    }
    const Actor* PursuedLocal(const uint8_t* base, uint32_t pursuit)
    {
        const auto* racer = RacerBySimable(FormalTarget(base, pursuit));
        return racer && racer->local ? racer : nullptr;
    }
    Assignment* FindAssignment(uint32_t ai)
    {
        for (auto& assignment : world.assignments)
            if (assignment.ai == ai) return &assignment;
        return nullptr;
    }
    Assignment* Track(Id id, uint32_t ai, uint32_t pursuit)
    {
        if (auto* entry = FindAssignment(ai))
        {
            if (entry->cop != id || entry->pursuit != pursuit)
                *entry = {id, 0, ai, pursuit};
            return entry;
        }
        for (auto& entry : world.assignments)
            if (!entry.cop)
            {
                entry = {id, 0, ai, pursuit};
                return &entry;
            }
        return nullptr;
    }
    Id Choose(const uint8_t* base, uint32_t pursuit, const Position* origin, const Assignment* exclude = nullptr)
    {
        std::array<Candidate, kMaxRacers> candidates{};
        size_t count = 0;
        for (size_t i = 0; i < world.count; ++i)
        {
            const auto& racer = world.racers[i];
            if (world.splitRace && racer.local)
            {
                const uint32_t own = OwnPursuit(base, racer);
                if (own && own != pursuit) continue;
            }
            unsigned coverage = 0;
            for (const auto& entry : world.assignments)
                coverage += &entry != exclude && entry.racer == racer.id;
            candidates[count++] = {racer.id, racer.position, coverage};
        }
        return Dispatch(std::span(candidates).first(count), origin);
    }
    bool PlayerCovered(Id player, uint32_t pursuit, const Assignment* exclude = nullptr)
    {
        for (const auto& entry : world.assignments)
            if (&entry != exclude && entry.racer == player && entry.pursuit == pursuit)
                return true;
        return false;
    }
    bool Acquire(uint8_t* base, uint32_t ai, uint32_t simable, float sightAge = 8.0f)
    {
        const uint32_t target = Read32(base, ai + 0x68);
        if (!Pointer(target, 0x50) || Read32(base, target) != 0x82064E1C) return false;
        if (TargetSimable(base, target) == simable) return true;
        GuestToHostFunction<void>(0x8210E118u, target, simable);
        if (TargetSimable(base, target) != simable) return false;
        WriteFloat(base, ai + 0x850, sightAge);
        base[ai + 0x864] = 0;
        return true;
    }
    void Snapshot(uint8_t* base, uint32_t manager)
    {
        world.count = 0;
        world.pendingContact = 0;
        if (!Pointer(manager) || Read32(base, manager) != 0x82065C9C)
        {
            world = {};
            return;
        }
        const uint32_t count = Read32(base, 0x82C6F194), data = Read32(base, 0x82C6F18C);
        if (count > 512 || !Pointer(data, count * 4)) { world = {}; return; }
        const bool split = game::split_screen::GameSaysSplit();
        if (split && !IsCompetitiveRace(base)) { world = {}; return; }
        const uint32_t splitRace = split ? Read32(base, 0x82A2CB18) : 0;
        Id player = 0;
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t vehicle = Read32(base, data + i * 4);
            if (!Pointer(vehicle) || Read32(base, vehicle) != 0x8208AD18) continue;
            const uint32_t driver = Read32(base, vehicle + 0x9C);
            if ((driver != 0 && driver != 3) || !Live(base, vehicle)) continue;
            if (splitRace)
            {
                const uint32_t racer = GuestToHostFunction<uint32_t>(0x8232F7F0u, splitRace, Simable(vehicle));
                if (!Pointer(racer, 0x22) || std::any_of(base + racer + 0x1C, base + racer + 0x22,
                    [](uint8_t flag) { return flag != 0; })) continue;
            }
            Actor racer{Identity(vehicle), vehicle, Read32(base, vehicle + 0x54)};
            if (!racer.id || !ReadPosition(base, vehicle, racer.position)) continue;
            racer.local = driver == 0;
            if (world.count == world.racers.size() || (racer.local && player && !splitRace)) { world = {}; return; }
            world.racers[world.count++] = racer;
            if (racer.local && !player) player = racer.id;
        }
        const float now = ReadFloat(base, 0x82A153B8);
        if (!player || !std::isfinite(now)) { world = {}; return; }
        if (world.manager != manager || world.splitRace != splitRace || (!splitRace && world.player != player) || now < world.time)
        {
            world.assignments = {};
            world.probeCursor = 0;
        }
        world.manager = manager;
        world.splitRace = splitRace;
        world.player = player;
        world.time = now;
        world.playerPursuit = OwnPursuit(base, *Racer(player));
    }
    void Assign(uint8_t* base)
    {
        const uint32_t count = Read32(base, 0x82C6F314), data = Read32(base, 0x82C6F30C);
        if (!world.player || count > kMaxCops || !Pointer(data, count * 4)) return;
        std::array<Actor, kMaxCops> cops{};
        size_t size = 0;
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t vehicle = Read32(base, data + 4 * i);
            if (!Live(base, vehicle)) continue;
            const uint32_t ai = Read32(base, vehicle + 0x54);
            if (!CopAI(base, ai) || !Pursuit(base, Read32(base, ai + 0x84))) continue;
            const Id id = Identity(vehicle);
            if (id) cops[size++] = {id, vehicle, ai};
        }
        const auto live = std::span(cops).first(size);
        for (auto& entry : world.assignments)
        {
            if (std::none_of(live.begin(), live.end(), [&](const Actor& cop) {
                return cop.id == entry.cop && cop.ai == entry.ai;
            })) entry = {};
        }
        for (const auto& cop : live)
        {
            const uint32_t pursuit = Read32(base, cop.ai + 0x84);
            if (!Pursuit(base, pursuit)) continue;
            auto* entry = Track(cop.id, cop.ai, pursuit);
            if (!entry) continue;
            const auto* target = Racer(entry->racer);
            if (!target)
            {
                const auto* stock = RacerBySimable(TargetSimable(base, Read32(base, cop.ai + 0x68)));
                const auto* local = world.splitRace ? PursuedLocal(base, pursuit) : nullptr;
                // Native contact already chose a driver. Only a free reinforcement
                // or a unit whose previous target disappeared needs dispatch.
                // In split screen stock also points every new unit in a local
                // pursuit at its owner, so dispatch those after the first one.
                if (!entry->racer && !entry->reinforcement && stock && (!local || stock != local))
                    target = stock;
                else if (local && !PlayerCovered(local->id, pursuit, entry))
                    target = local;
                else if (!world.splitRace && pursuit == world.playerPursuit &&
                    !PlayerCovered(world.player, pursuit, entry))
                    target = Racer(world.player);
                else
                {
                    Position position;
                    if (ReadPosition(base, cop.vehicle, position)) target = Racer(Choose(base, pursuit, &position, entry));
                    if (!target) target = stock;
                }
            }
            if (target && Acquire(base, cop.ai, Simable(target->vehicle)))
            {
                if (entry->racer != target->id) LogSplitAssignment(cop.vehicle, *target, pursuit);
                entry->racer = target->id;
                entry->reinforcement = false;
            }
        }
    }
    uint32_t PlayerProbeTarget(uint8_t* base, uint32_t simable)
    {
        static thread_local const uint32_t scratch = [] {
            auto* memory = g_userHeap.Alloc(0x50);
            return memory ? g_memory.MapVirtual(memory) : 0u;
        }();
        if (!scratch) return 0;
        std::memset(base + scratch, 0, 0x50);
        const uint32_t vt = ByteSwap(0x82064E1Cu);
        std::memcpy(base + scratch, &vt, 4);
        GuestToHostFunction<void>(0x8210E118u, scratch, simable);
        return TargetSimable(base, scratch) == simable ? scratch : 0;
    }
    void ProbePlayer(uint8_t* base)
    {
        const auto* player = Racer(world.player);
        if (!player) return;
        const bool allowRequest = !world.splitRace && !world.playerPursuit && IsCompetitiveRace(base);
        if (!world.splitRace && !world.playerPursuit && !allowRequest) return;
        Id cachedPlayer = 0;
        uint32_t target = 0;
        unsigned probes = 0;
        for (size_t i = 0; i < kMaxCops && probes < 8; ++i)
        {
            auto& entry = world.assignments[world.probeCursor++ % kMaxCops];
            if (!entry.cop || !entry.racer ||
                (entry.lastProbe >= 0 && world.time - entry.lastProbe < 0.25f) ||
                !CopAI(base, entry.ai) || Read32(base, entry.ai + 0x84) != entry.pursuit ||
                FormalTarget(base, entry.pursuit) == 0) continue;
            const auto* owner = world.splitRace ? PursuedLocal(base, entry.pursuit) : player;
            if (!owner || entry.racer == owner->id) continue;
            const auto* driver = Racer(entry.racer);
            const uint32_t cop = Read32(base, entry.ai - 0x1C) + 0x94;
            if (!driver || !Live(base, cop) || Read32(base, cop + 0x54) != entry.ai ||
                TargetSimable(base, Read32(base, entry.ai + 0x68)) != Simable(driver->vehicle)) continue;
            entry.lastProbe = world.time;
            if (cachedPlayer != owner->id)
            {
                target = PlayerProbeTarget(base, Simable(owner->vehicle));
                cachedPlayer = owner->id;
            }
            if (!target) return;
            if (entry.probedPlayer != owner->id)
            {
                entry.probedPlayer = owner->id;
                entry.playerHidden = 0;
                entry.seenPlayer = -1;
            }
            ++probes;
            auto& hidden = base[entry.ai + 0x864];
            const uint8_t saved = hidden;
            hidden = entry.playerHidden;
            const bool visible = GuestToHostFunction<uint32_t>(0x82107F50u, entry.ai + 0x800, target) & 0xFF;
            entry.playerHidden = hidden;
            hidden = saved;
            if (!visible) continue;
            entry.seenPlayer = world.time;
            if (allowRequest && !world.pendingContact) world.pendingContact = entry.ai;
        }
    }
}

bool IsCompetitiveRace(uint8_t* base)
{
    const uint32_t race = Read32(base, 0x82A2CB18);
    if (!Pointer(race, 0x1A34) || Read32(base, race + 0x1A28) != 1 || base[race + 0x1A24]) return false;
    const uint32_t parms = Read32(base, race + 0x1A30);
    if (!Pointer(parms) || !(GuestToHostFunction<uint32_t>(0x823393B8u, parms) & 0xFF) ||
        (GuestToHostFunction<uint32_t>(0x82337E60u, parms) & 0xFF) ||
        (GuestToHostFunction<uint32_t>(0x82337C80u, parms) & 0xFF)) return false;
    return GuestToHostFunction<uint32_t>(0x8233A028u, parms) <= 8;
}

uint32_t SpawnAnchor(uint8_t* base, uint32_t pursuit)
{
    if (!Enabled() || world.count < 2 || !Pursuit(base, pursuit)) return 0;
    if (const auto* owner = PursuedLocal(base, pursuit);
        owner && (OwnPursuit(base, *owner) != pursuit || !PlayerCovered(owner->id, pursuit))) return 0;
    const auto* racer = Racer(Choose(base, pursuit, nullptr));
    return racer && Live(base, racer->vehicle) ? racer->vehicle : 0;
}

void Spawned(uint8_t* base, uint32_t cop, uint32_t anchor)
{
    if (!Enabled() || !Live(base, cop)) return;
    const uint32_t ai = Read32(base, cop + 0x54);
    if (!CopAI(base, ai)) return;
    const Id id = Identity(cop);
    const uint32_t pursuit = Read32(base, ai + 0x84);
    if (!id || !Pursuit(base, pursuit)) return;
    if (auto* entry = Track(id, ai, pursuit))
    {
        *entry = {id, 0, ai, pursuit};
        entry->reinforcement = true;
        const auto* racer = anchor ? RacerBySimable(Simable(anchor)) : nullptr;
        if (racer && Live(base, anchor) && Acquire(base, ai, Simable(anchor)))
        {
            entry->racer = racer->id;
            entry->reinforcement = false;
            LogSplitAssignment(cop, *racer, pursuit);
        }
    }
}
}

// PVehicle constructor/destructor: record native lifetime IDs around originals.
PPC_FUNC(sub_820F0270)
{
    using namespace game::fair_cops;
    __imp__sub_820F0270(ctx, base);
    if (!Enabled()) return;
    if (!s_log)
    {
        Assign(base);
        return;
    }
    using Clock = std::chrono::steady_clock;
    static thread_local auto last = Clock::now();
    static thread_local double total = 0, worst = 0;
    static thread_local unsigned samples = 0;
    const auto start = Clock::now();
    Assign(base);
    const auto end = Clock::now();
    const double elapsed = std::chrono::duration<double, std::micro>(end - start).count();
    total += elapsed;
    worst = std::max(worst, elapsed);
    ++samples;
    if (end - last >= std::chrono::seconds(5))
    {
        fprintf(stderr, "[fair-cops] racers=%zu avg_us=%.1f max_us=%.1f\n",
            world.count, total / samples, worst);
        total = worst = 0;
        samples = 0;
        last = end;
    }
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

extern "C" PPC_FUNC(__imp__sub_820EE908);
extern "C" PPC_FUNC(__imp__sub_8210B2E8);
extern "C" PPC_FUNC(__imp__sub_820E9348);
extern "C" PPC_FUNC(__imp__sub_82117950);
extern "C" PPC_FUNC(__imp__sub_820E9418);

PPC_FUNC(sub_820EE908)
{
    using namespace game::fair_cops;
    if (Enabled() && ctx.lr == 0x820F2378)
    {
        Snapshot(base, ctx.r3.u32);
        ProbePlayer(base);
    }
    __imp__sub_820EE908(ctx, base);
    world.pendingContact = 0;
}

PPC_FUNC(sub_8210B2E8)
{
    using namespace game::fair_cops;
    const uint32_t interface = ctx.r3.u32, caller = ctx.lr;
    __imp__sub_8210B2E8(ctx, base);
    if (ctx.r3.u32 || caller != 0x820EEEA4 || !world.pendingContact ||
        interface != world.pendingContact + 0x800) return;

    const uint32_t ai = world.pendingContact;
    world.pendingContact = 0;
    const auto* player = Racer(world.player);
    if (!player || !Live(base, player->vehicle) || !CopAI(base, ai)) return;
    const uint32_t pursuit = Read32(base, ai + 0x84);
    const uint32_t prior = TargetSimable(base, Read32(base, ai + 0x68));
    const uint32_t copSimable = Read32(base, ai - 0x1C);
    const uint32_t cop = copSimable + 0x94;
    if (!prior || !Live(base, cop) || Read32(base, cop + 0x54) != ai ||
        !FormalTarget(base, pursuit) || FormalTarget(base, pursuit) == Simable(player->vehicle)) return;

    if (!Acquire(base, ai, Simable(player->vehicle), -0.25f)) return;
    if (!(GuestToHostFunction<uint32_t>(0x820E9770u, pursuit, cop) & 0xFF))
    {
        Acquire(base, ai, prior);
        return;
    }
    // The stock manager consumes this request, attaches the local driver and
    // joins the cop through AddVehicle. The original RemoveVehicle ended its
    // previous pursuit and cleared its membership.
    ctx.r3.u64 = Read32(base, ai + 0x68);
    if (s_log) fprintf(stderr, "[fair-cops] player contact cop=%08X\n", cop);
}

PPC_FUNC(sub_820E9348)
{
    using namespace game::fair_cops;
    if (Enabled() && !ctx.r4.u8)
        if (auto* entry = FindAssignment(ctx.r3.u32 - 0x800)) *entry = {};
    __imp__sub_820E9348(ctx, base);
}

PPC_FUNC(sub_82117950)
{
    using namespace game::fair_cops;
    const uint32_t task = ctx.r3.u32;
    const uint32_t saved = sightTarget;
    if (Enabled() && Pointer(task) && ctx.r4.u32 == Read32(base, task + 0x4C))
    {
        sightTarget = FormalTarget(base, task + 0x40);
        const auto* player = world.splitRace ? PursuedLocal(base, task + 0x40) : Racer(world.player);
        if (player && sightTarget == Simable(player->vehicle) &&
            (world.splitRace || task + 0x40 == world.playerPursuit))
        {
            const float now = ReadFloat(base, 0x82A153B8);
            float age = ReadFloat(base, task + 0x168);
            for (const auto& entry : world.assignments)
            {
                if (!entry.cop || entry.probedPlayer != player->id || (world.splitRace && entry.pursuit != task + 0x40) ||
                    entry.seenPlayer < 0 || now < entry.seenPlayer ||
                    now - entry.seenPlayer > 0.5f || !CopAI(base, entry.ai) ||
                    Read32(base, entry.ai + 0x84) != entry.pursuit) continue;
                age = std::min(age, now - entry.seenPlayer);
            }
            WriteFloat(base, task + 0x168, age);
        }
    }
    __imp__sub_82117950(ctx, base);
    sightTarget = saved;
}

PPC_FUNC(sub_820E9418)
{
    using namespace game::fair_cops;
    const uint32_t interface = ctx.r3.u32, caller = ctx.lr;
    __imp__sub_820E9418(ctx, base);
    if (sightTarget && caller == 0x8211816C && CopAI(base, interface - 0x800) &&
        TargetSimable(base, Read32(base, interface - 0x800 + 0x68)) != sightTarget)
        ctx.f1.f64 = std::numeric_limits<float>::max();
}
