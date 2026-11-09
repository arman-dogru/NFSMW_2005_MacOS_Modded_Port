// SpeedBreaker. GPL-3.0-or-later (see COPYING).
#include <stdafx.h>
#include "auto_drive.h"
#include "fair_cops.h"
#include <kernel/function.h>
#include <region/region.h>

namespace game::auto_drive
{
namespace
{
    std::atomic<unsigned> pending{0};
    // Only the game update thread accesses the ownership record.
    uint32_t ownedInterface = 0;
    uint32_t ownedRacer = 0;

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
    uint32_t LocalRacer(uint8_t* base)
    {
        if (Read32(base, 0x82C6F1F4) != 1 || !fair_cops::IsCompetitiveRace(base)) return 0;
        const uint32_t list = Read32(base, 0x82C6F1EC);
        if (!Pointer(list, 4)) return 0;
        const uint32_t vehicle = Read32(base, list);
        if (!Pointer(vehicle, 0xB8) || Read32(base, vehicle) != 0x8208AD18 ||
            Read32(base, vehicle + 0x9C) != 0 || !Read32(base, vehicle + 0xB4)) return 0;
        return GuestToHostFunction<uint32_t>(0x8232F7F0u, Read32(base, 0x82A2CB18), vehicle - 0x94);
    }
    uint32_t ControlInterface(const uint8_t* base)
    {
        // The debug command treats 0 as the first local player. Use that
        // same lookup, and require one player so controller ownership is clear.
        if (Read32(base, 0x82C74BDC) != 1) return 0;
        const uint32_t element = GuestToHostFunction<uint32_t>(0x82366FD8u, 0u);
        if (!Pointer(element, 8)) return 0;
        const uint32_t object = Read32(base, element + 4);
        if (!Pointer(object, 4)) return 0;
        return GuestToHostFunction<uint32_t>(0x8231AF60u, object, 0x820E3D98u);
    }
}

void Toggle()
{
    pending.fetch_xor(1, std::memory_order_relaxed);
}

void Update(uint8_t* base)
{
    const bool toggle = pending.exchange(0, std::memory_order_relaxed);
    if ((!toggle && !ownedInterface) || region::Current() != region::Release::Usa) return;

    const uint32_t racer = LocalRacer(base);
    bool eligible = Pointer(racer, 0x22);
    if (eligible)
        for (uint32_t flag = 0x1C; flag <= 0x21; ++flag)
            eligible &= base[racer + flag] == 0;
    if (!toggle && eligible && racer == ownedRacer) return;

    const uint32_t control = ControlInterface(base);
    const uint32_t vtable = Pointer(control, 4) ? Read32(base, control) : 0;
    if (!Pointer(vtable, 0x14))
    {
        ownedInterface = ownedRacer = 0;
        return;
    }
    const uint32_t getter = Read32(base, vtable + 0x0C);
    if ((getter & 3) || getter < PPC_CODE_BASE || getter >= PPC_CODE_BASE + PPC_CODE_SIZE ||
        !g_memory.FindFunction(getter)) return;

    if (ownedInterface == control && (!eligible || racer != ownedRacer || toggle))
    {
        // Finished racers are handed to the stock finish logic. In the
        // other cases release our AI override while the interface is live.
        if (racer != ownedRacer || eligible)
        {
            GuestToHostFunction<void>(0x823671E8u, 0u);
            if (GuestToHostFunction<uint32_t>(getter, control) & 0xFF) return;
        }
        ownedInterface = ownedRacer = 0;
        fprintf(stderr, "[auto-drive] released player 1\n");
        return;
    }
    if (ownedInterface != control) ownedInterface = ownedRacer = 0;
    if (!toggle || !eligible || (GuestToHostFunction<uint32_t>(getter, control) & 0xFF)) return;
    GuestToHostFunction<void>(0x82367150u, 0u);
    if (GuestToHostFunction<uint32_t>(getter, control) & 0xFF)
    {
        ownedInterface = control;
        ownedRacer = racer;
        fprintf(stderr, "[auto-drive] engaged player 1\n");
    }
}
}
