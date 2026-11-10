// SpeedBreaker. GPL-3.0-or-later (see COPYING).
#include <stdafx.h>
#include "split_screen.h"
#include <kernel/function.h>
#include <kernel/heap.h>
#include <kernel/memory.h>
#include <region/region.h>

extern "C" PPC_FUNC(__imp__sub_822BCE58);

namespace
{
    constexpr uint32_t kRaceTypeOffset = 0x18;
    constexpr uint32_t kChallengeType = 8;
    constexpr uint32_t kPursuitEvent = 0x7E8EDB81;
    constexpr uint32_t kChaseLabel = 0xCF10C0DE;

    uint32_t Read32(const uint8_t* base, uint32_t address)
    {
        return PPC_LOAD_U32(address);
    }

    bool SplitScreenUSA()
    {
        return region::Current() == region::Release::Usa && game::split_screen::GameSaysSplit();
    }
}

bool CooperativeChaseSelected()
{
    if (!SplitScreenUSA()) return false;
    const auto* base = g_memory.base;
    const uint32_t db = Read32(base, game::split_screen::kDatabasePointer);
    return Read32(base, db + kRaceTypeOffset) == kChallengeType;
}

void CooperativeChaseShowOption(PPCRegister& skipKnockout)
{
    if (SplitScreenUSA()) skipKnockout.u64 = 0;
}

void CooperativeChaseOption(PPCRegister& type, PPCRegister& label)
{
    if (!SplitScreenUSA()) return;
    type.u64 = kChallengeType;
    label.u64 = kChaseLabel;
}

bool CooperativeChaseEvent(PPCRegister& menu, PPCRegister& list)
{
    if (!CooperativeChaseSelected()) return false;
    auto* base = g_memory.base;
    const uint32_t manager = Read32(base, 0x82A2CB1C);
    if (!game::split_screen::PlausibleObject(manager)) return false;
    const uint32_t event = GuestToHostFunction<uint32_t>(0x82336830u, manager, kPursuitEvent);
    if (!event || !(GuestToHostFunction<uint32_t>(0x82337E60u, event) & 0xFF)) return false;

    // The original list builder has cleared its intrusive list. Add a
    // single, unlocked entry and let its normal selection code take over.
    const uint32_t head = list.u32;
    const uint32_t node = GuestToHostFunction<uint32_t>(0x8215CB08u, 20u, 0u, 0u, 0u);
    if (!node) return false;
    PPC_STORE_U32(node, head);
    PPC_STORE_U32(node + 4, head);
    PPC_STORE_U32(node + 8, event);
    PPC_STORE_U32(node + 12, 0);
    PPC_STORE_U32(node + 16, 0);
    PPC_STORE_U32(head, node);
    PPC_STORE_U32(head + 4, node);
    PPC_STORE_U32(menu.u32 + 260, node);
    return true;
}

void CooperativeChaseCarSelection(PPCRegister& type)
{
    // This is only the comparison that routes Drag to the two-car selector.
    // The actual event still has its original Challenge Series race type.
    if (type.u32 == kChallengeType && CooperativeChaseSelected()) type.u64 = 2;
}

PPC_FUNC(sub_822BCE58)
{
    if (ctx.r3.u32 == kChaseLabel && SplitScreenUSA())
    {
        static const uint32_t label = [] {
            constexpr char text[] = "Cooperative Chase";
            void* guest = g_userHeap.Alloc(sizeof(text));
            if (!guest) return 0u;
            std::memcpy(guest, text, sizeof(text));
            return g_memory.MapVirtual(guest);
        }();
        if (label)
        {
            ctx.r3.u64 = label;
            return;
        }
    }
    __imp__sub_822BCE58(ctx, base);
}
