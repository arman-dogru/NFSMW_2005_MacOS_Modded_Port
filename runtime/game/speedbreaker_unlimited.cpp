// SpeedBreaker. GPL-3.0-or-later (see COPYING).
// Verified USA charge/toggle/eligibility paths: docs/SPEEDBREAKER_UNLIMITED.md.
#include <stdafx.h>
#include <region/region.h>
#include <user/settings.h>

extern "C" PPC_FUNC(__imp__sub_823D2C48);
extern "C" PPC_FUNC(__imp__sub_8246C0A0);
extern "C" PPC_FUNC(__imp__sub_8246C150);
extern "C" PPC_FUNC(__imp__sub_8246C188);

namespace
{
    constexpr uint32_t kInterfaceOffset = 0x4C;
    constexpr uint32_t kChargeOffset = 0x84;
    constexpr uint32_t kActiveOffset = 0x80;
    constexpr uint32_t kEligibilitySpeedCaller = 0x8246C004;

    uint32_t Read32(const uint8_t* base, uint32_t addr)
    {
        uint32_t n;
        std::memcpy(&n, base + addr, 4);
        return ByteSwap(n);
    }
    bool Pointer(uint32_t p, uint32_t extent)
    {
        return !(p & 3) && p >= 0x10000 && uint64_t(p) + extent < 0x100000000ull;
    }
    bool Enabled()
    {
        return region::Current() == region::Release::Usa &&
            settings::GetBool(settings::Id::UnlimitedSpeedbreaker);
    }
    bool LocalPlayer(const uint8_t* base, uint32_t player)
    {
        return Pointer(player, 0x88) && Read32(base, player) == 0x820928F0 &&
            Read32(base, player + kInterfaceOffset) == 0x82092948;
    }
    void Fill(uint8_t* base, uint32_t player)
    {
        const uint32_t one = ByteSwap(uint32_t{0x3F800000});
        std::memcpy(base + player + kChargeOffset, &one, sizeof(one));
    }
    void Log(const uint8_t* base, uint32_t player, const char* action, bool force = false)
    {
        static const bool enabled = [] {
            const char* v = std::getenv("NFSMW_SB_LOG");
            return v && std::strcmp(v, "1") == 0;
        }();
        if (!enabled) return;
        static unsigned lines = 0;
        static auto next = std::chrono::steady_clock::time_point{};
        const auto now = std::chrono::steady_clock::now();
        if (lines >= 256 || (!force && now < next)) return;
        next = now + std::chrono::seconds(5);
        ++lines;
        const uint32_t bits = Read32(base, player + kChargeOffset);
        float charge;
        std::memcpy(&charge, &bits, sizeof(charge));
        fprintf(stderr, "[speedbreaker-unlimited] %s enabled=%u active=%u charge=%.6f\n",
            action, unsigned(Enabled()), unsigned(base[player + kActiveOffset] != 0), charge);
    }
}

// IVehicle::GetSpeed, slot 35. Change only the value returned to the
// eligibility check: 14 m/s clears its 30 MPH threshold (factor 2.23699).
// The stored speed, physics, HUD, recharge-speed read and every other caller
// retain the real speed. The rest of CanUseSpeedbreaker's checks still run.
PPC_FUNC(sub_823D2C48)
{
    const uint32_t caller = ctx.lr;
    __imp__sub_823D2C48(ctx, base);
    if (caller == kEligibilitySpeedCaller && Enabled() &&
        Pointer(ctx.r3.u32, 0x7C) && Read32(base, ctx.r3.u32) == 0x8208AD18 &&
        ctx.f1.f64 < 14.0)
        ctx.f1.f64 = 14.0;
}

// Input toggle receives the ISpeedbreaker interface at LocalPlayer+0x4C.
// Fill before the original so even a completely empty meter can activate on
// the very first input. Original eligibility and manual deactivation remain.
PPC_FUNC(sub_8246C0A0)
{
    const uint32_t player = ctx.r3.u32 - kInterfaceOffset;
    const bool valid = LocalPlayer(base, player);
    if (valid && Enabled()) Fill(base, player);
    __imp__sub_8246C0A0(ctx, base);
    if (valid) Log(base, player, "toggle", true);
}

// Reset must still turn Speedbreaker off at race/vehicle transitions.
PPC_FUNC(sub_8246C150)
{
    const uint32_t player = ctx.r3.u32 - kInterfaceOffset;
    const bool unlimited = Enabled() && LocalPlayer(base, player);
    __imp__sub_8246C150(ctx, base);
    if (unlimited) Fill(base, player);
}

// f1/f2 are elapsed recharge/drain time used only by this charge updater,
// not world/physics time. Keep the original vehicle/cinematic checks and
// off-event cleanup; suppress drain and replenish immediately when enabled.
PPC_FUNC(sub_8246C188)
{
    const uint32_t player = ctx.r3.u32;
    const bool valid = LocalPlayer(base, player);
    const bool unlimited = valid && Enabled();
    if (unlimited)
    {
        Fill(base, player);
        ctx.f1.f64 = 0;
        ctx.f2.f64 = 0;
    }
    __imp__sub_8246C188(ctx, base);
    if (unlimited) Fill(base, player);
    if (valid) Log(base, player, "update");
}
