// SpeedBreaker. GPL-3.0-or-later (see COPYING).
#include <stdafx.h>
#include "auto_drive.h"
#include "cooperative_chase.h"
#include "fair_cops.h"
#include "split_screen.h"
#include <kernel/function.h>
#include <region/region.h>

namespace game::auto_drive
{
namespace
{
    std::atomic<unsigned> pending{0};
    // Only the game update thread accesses these ownership records.
    uint32_t ownedVehicle[2] = {};

    uint32_t Read32(const uint8_t* base, uint32_t address)
    {
        uint32_t value;
        std::memcpy(&value, base + address, sizeof(value));
        return ByteSwap(value);
    }

    bool Pointer(uint32_t address, uint32_t size)
    {
        return !(address & 3) && address >= 0x10000 && uint64_t(address) + size < 0x100000000ull;
    }

    uint32_t ControlInterface(const uint8_t* base, unsigned player)
    {
        const uint32_t element = GuestToHostFunction<uint32_t>(0x82366FD8u, player);
        if (!Pointer(element, 8)) return 0;
        const uint32_t object = Read32(base, element + 4);
        if (!Pointer(object, 4)) return 0;
        const uint32_t control = GuestToHostFunction<uint32_t>(0x8231AF60u, object, 0x820E3D98u);
        return Pointer(control, 12) && Read32(base, control) == 0x82066354 ? control : 0;
    }

    uint32_t LocalVehicle(const uint8_t* base, uint32_t control, unsigned players)
    {
        const uint32_t count = Read32(base, 0x82C6F1F4);
        const uint32_t list = Read32(base, 0x82C6F1EC);
        if (count != players || !Pointer(list, count * 4)) return 0;
        for (unsigned i = 0; i < count; ++i)
        {
            const uint32_t vehicle = Read32(base, list + i * 4);
            if (Pointer(vehicle, 0xB8) && Read32(base, vehicle) == 0x8208AD18 &&
                Read32(base, vehicle + 0x9C) == 0 &&
                Read32(base, vehicle + 0x54) == control - 0x880)
                return vehicle;
        }
        return 0;
    }

    bool RaceHandedOff(uint8_t* base, uint32_t vehicle)
    {
        if (!fair_cops::IsCompetitiveRace(base)) return false;
        const uint32_t racer = GuestToHostFunction<uint32_t>(0x8232F7F0u, Read32(base, 0x82A2CB18), vehicle - 0x94);
        if (!Pointer(racer, 0x22)) return true;
        for (uint32_t flag = 0x1C; flag <= 0x21; ++flag)
            if (base[racer + flag]) return true;
        return false;
    }
}

void Toggle(unsigned user)
{
    if (user < 4) pending.fetch_xor(1u << user, std::memory_order_relaxed);
}

void Update(uint8_t* base)
{
    const unsigned toggles = pending.exchange(0, std::memory_order_relaxed);
    if ((!toggles && !ownedVehicle[0] && !ownedVehicle[1]) || region::Current() != region::Release::Usa)
        return;

    const uint32_t database = Read32(base, split_screen::kDatabasePointer);
    const bool split = split_screen::PlausibleObject(database) &&
        split_screen::Decode(base[database], Read32(base, database + split_screen::kFlagsOffset));
    const unsigned players = Read32(base, 0x82C74BDC);
    if (players != (split ? 2u : 1u))
    {
        ownedVehicle[0] = ownedVehicle[1] = 0;
        if (toggles && split)
            fprintf(stderr, "[auto-drive] ignored split request: %u registered local players\n", players);
        return;
    }

    for (unsigned player = 0; player < players; ++player)
    {
        // The game's join screen stores the chosen XInput ports at +12/+13.
        const unsigned user = split ? base[database + 12 + player] : 0;
        const bool toggle = user < 4 && (toggles & (1u << user));
        auto& owned = ownedVehicle[player];
        if (!toggle && !owned) continue;

        const uint32_t control = ControlInterface(base, player);
        const uint32_t vehicle = control ? LocalVehicle(base, control, players) : 0;
        if (!vehicle || RaceHandedOff(base, vehicle))
        {
            owned = 0;
            if (toggle && split)
                fprintf(stderr, "[auto-drive] ignored player %u request (control=%08X, vehicle=%08X)\n",
                    player + 1, control, vehicle);
            continue;
        }
        if (owned != vehicle || !base[control + 8]) owned = 0;
        if (!toggle || !Read32(base, vehicle + 0xB4)) continue;

        const bool aiDriving = base[control + 8] != 0;
        if (aiDriving && !owned && !cooperative_chase::Active(base)) continue;
        const bool enable = !aiDriving;
        GuestToHostFunction<void>(0x820FF938u, control, unsigned(enable));
        if (bool(base[control + 8]) == enable)
        {
            owned = enable ? vehicle : 0;
            fprintf(stderr, "[auto-drive] %s player %u (user %u, vehicle %08X)\n",
                enable ? "engaged" : "released", player + 1, user, vehicle);
        }
    }

    if (!split) ownedVehicle[1] = 0;
}
}
