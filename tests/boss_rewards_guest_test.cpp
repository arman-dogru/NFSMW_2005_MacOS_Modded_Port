// SpeedBreaker. GPL-3.0-or-later (see COPYING).
// Run with boss_rewards_guest_test.py after building the native executable.
// Original guest bodies run against synthetic FE state; no saves/assets loaded.
#include "ppc_recomp_shared.h"
#include <game/boss_rewards.h>
#include <region/region.h>
#include <user/settings.h>
#include <array>
#include <cassert>
#include <cstdio>
#include <sys/mman.h>

namespace settings::detail
{
    std::atomic<uint8_t> g_state{2};
    std::atomic<double> g_effective[kCount];
    void InitBeforeLoad(Id) { assert(false); }
}
extern "C" PPC_FUNC(__imp__sub_82586768);
extern "C" PPC_FUNC(__imp__sub_82586E28);
extern "C" PPC_FUNC(__imp__sub_82586E78);

namespace
{
    constexpr uint32_t screen = 0x10000, db = 0x20000, career = 0x30000;
    constexpr uint32_t inventory = 0x82A391B0, cash = career + 0xBC;
    unsigned choice, pinkSlips, continueCalls, labelUpdates;
    int expectedRemaining;
    uint32_t W(const uint8_t* base, uint32_t p) { return __builtin_bswap32(*reinterpret_cast<const uint32_t*>(base + p)); }
    void S(uint8_t* base, uint32_t p, uint32_t n) { *reinterpret_cast<uint32_t*>(base + p) = __builtin_bswap32(n); }
    void NoOp(PPCContext&, uint8_t*) {}
    void Pick(uint8_t* base, unsigned index)
    {
        choice = index;
        PPCContext ctx{};
        ctx.r1.u64 = 0x100000;
        ctx.r3.u64 = screen;
        ctx.r4.u64 = 0x0C407210; // original pick-card notification
        ctx.r5.u64 = 0x40000; // dummy FE object; animation API is mocked
        __imp__sub_82586768(ctx, base);
    }
    void Draw(uint8_t* base, int remaining)
    {
        expectedRemaining = remaining;
        const unsigned before = labelUpdates;
        PPCContext ctx{};
        ctx.r1.u64 = 0x100000;
        ctx.r3.u64 = screen;
        __imp__sub_82586E78(ctx, base);
        assert(labelUpdates == before + 2); // both original formatted labels
    }
}

PPC_FUNC(sub_82586E78) { __imp__sub_82586E78(ctx, base); }
PPC_FUNC(sub_82586B80) { ctx.r3.u64 = choice; } // selected UI button
PPC_FUNC(sub_82586B20) { ctx.r3.u64 = choice; }
PPC_FUNC(sub_822CB178) { assert(ctx.r4.u32 == 15); ++pinkSlips; } // boss-car award API
PPC_FUNC(sub_82284E70) { ++continueCalls; }
PPC_FUNC(sub_8227FD58) { ctx.r3.u64 = 0; } // no actual FE images in this test
PPC_FUNC(sub_822BCE58) { ctx.r3.u64 = 0x50000; }
PPC_FUNC(sub_822DADB8) { ctx.r3.u64 = 1; }
PPC_FUNC(sub_822BCFA8) { base[ctx.r3.u32] = 0; }
PPC_FUNC(sub_82282840)
{
    if (ctx.r4.u32 == 0xEB0A8ABD) return; // selected cash card's description
    assert(ctx.r4.u32 == 0x38DEAC6B || ctx.r4.u32 == 0xD6C0E097);
    assert(ctx.r6.s32 == expectedRemaining);
    ++labelUpdates;
}
PPC_FUNC(sub_82586C48) { ctx.r3.u64 = 1; }
PPC_FUNC(sub_82586C98) { ctx.r3.u64 = 1; }
PPC_FUNC(sub_82586CE8) { ctx.r3.u64 = 1; }
PPC_FUNC(sub_82586D88) { ctx.r3.u64 = 1; }
PPC_FUNC(sub_822517B8) { NoOp(ctx, base); }
PPC_FUNC(sub_8227FF60) { NoOp(ctx, base); }
PPC_FUNC(sub_82282580) { NoOp(ctx, base); }
PPC_FUNC(sub_82282B10) { NoOp(ctx, base); }
PPC_FUNC(sub_82284CA8) { assert(false); } // never used by pick notification

int main()
{
    auto* base = static_cast<uint8_t*>(mmap(nullptr, PPC_MEMORY_SIZE, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    assert(base != MAP_FAILED);
    for (bool enabled : {false, true})
    {
        settings::detail::g_effective[size_t(settings::Id::AllBossRewards)] = enabled ? 1 : 0;
        S(base, screen, game::boss_rewards::kScreenVtable);
        S(base, screen + 0x2C, 6);
        S(base, 0x82A2C684, db);
        S(base, db + 0x10, career);
        S(base, cash, 100);
        std::memset(base + inventory, 0, 63 * 12);
        const std::array<uint32_t, 6> types{18, 19, 1, 8, 16, 20};
        const std::array<uint32_t, 6> params{15, 10000, 0, 0, 0, 0};
        for (unsigned i = 0; i < 6; ++i)
        {
            S(base, screen + 0x74 + i * 12, types[i]);
            S(base, screen + 0x78 + i * 12, params[i]);
            S(base, screen + 0x7C + i * 12, 0);
        }
        pinkSlips = continueCalls = labelUpdates = 0;
        const unsigned limit = enabled ? 6 : 2;
        Draw(base, limit);
        for (unsigned i = 0; i < limit; ++i)
        {
            Pick(base, i);
            assert(base[screen + 0x7C + i * 12] == 1);
            Draw(base, int(limit - i - 1));
            // Click an already selected card before the limit, then again at
            // the limit. Original selected flags must prevent double awards.
            Pick(base, i);
            assert(pinkSlips == 1 && W(base, cash) == (i ? 10100u : 100u));
        }
        assert(pinkSlips == 1 && W(base, cash) == 10100);
        assert(W(base, inventory) == (enabled ? 1u : 0u));
        if (enabled)
            for (unsigned i = 0; i < 4; ++i)
            {
                assert(W(base, inventory + i * 12) == types[i + 2]);
                assert(W(base, inventory + i * 12 + 8) == 1);
            }
        const auto before = continueCalls;
        Pick(base, enabled ? 0 : 2);
        assert(continueCalls == before + 1);
        if (!enabled) assert(base[screen + 0x7C + 2 * 12] == 0);
        // Unrelated callers still receive the real count, not a shifted value.
        PPCContext ctx{};
        ctx.r3.u64 = screen;
        ctx.lr = 0;
        sub_82586E28(ctx, base);
        assert(ctx.r3.u32 == limit);
        if (enabled)
        {
            ctx.r3.u64 = screen;
            ctx.lr = 0x82586868;
            S(base, screen, 0); // unrelated object layout must fail closed
            sub_82586E28(ctx, base);
            assert(ctx.r3.u32 == 6);
            S(base, screen, game::boss_rewards::kScreenVtable);
            ctx.r3.u64 = screen;
            region::detail::g_release = region::Release::PalEs;
            sub_82586E28(ctx, base);
            assert(ctx.r3.u32 == 6);
            region::detail::g_release = region::Release::Usa;
        }
        std::printf("guest rewards %s: %u picks, both labels, original cash/inventory writes, duplicate guard and continue passed\n",
            enabled ? "enabled" : "stock", limit);
    }
    assert(munmap(base, PPC_MEMORY_SIZE) == 0);
}
