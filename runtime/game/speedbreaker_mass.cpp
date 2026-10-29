// SpeedBreaker. GPL-3.0-or-later (see COPYING).
#include <stdafx.h>
#include "speedbreaker_mass.h"
#include <region/region.h>
#include <cerrno>

extern "C" PPC_FUNC(__imp__sub_82408380);

namespace
{
    const double s_multiplier = [] {
        const char* text = std::getenv("NFSMW_SB_MASS");
        if (!text) return 2.0;
        char* end = nullptr;
        errno = 0;
        const double n = std::strtod(text, &end);
        if (end == text || *end || errno == ERANGE || !std::isfinite(n) ||
            n < 0 || n > game::speedbreaker::kMaxMultiplier)
        {
            fprintf(stderr, "[speedbreaker] invalid NFSMW_SB_MASS; using stock 2\n");
            return 2.0;
        }
        return n;
    }();
    const bool s_log = [] {
        const char* v = std::getenv("NFSMW_SB_LOG");
        return v && std::strcmp(v, "1") == 0;
    }();
    uint32_t Read32(const uint8_t* base, uint32_t addr)
    {
        uint32_t v;
        std::memcpy(&v, base + addr, 4);
        return ByteSwap(v);
    }
    float ReadFloat(const uint8_t* base, uint32_t addr)
    {
        const uint32_t bits = Read32(base, addr);
        float n;
        std::memcpy(&n, &bits, 4);
        return n;
    }
}

// USA collision override interface slot 1 stores f1 at this +0x3C.
// Only the verified Speedbreaker activation caller may change its argument.
// Other callers and the zero-valued deactivation call retain original behavior.
PPC_FUNC(sub_82408380)
{
    const uint32_t object = ctx.r3.u32;
    const uint32_t caller = ctx.lr;
    const double original = ctx.f1.f64;
    const bool usa = region::Current() == region::Release::Usa;
    const bool activation = usa && caller == 0x82390BA8;
    const bool deactivation = usa && caller == 0x82390BD0;
    if (activation)
        ctx.f1.f64 = game::speedbreaker::ActivationMass(original, s_multiplier);
    __imp__sub_82408380(ctx, base);
    static unsigned lines = 0;
    if (s_log && (activation || deactivation) && lines++ < 64)
    {
        // The derived collision interface is root+0x130; its IRigidBody is
        // root+0x48. Follow only the body pool links used by GetMass (823DA900).
        const uint32_t body = object - 0xE8;
        const uint32_t poolRef = Read32(base, body + 0x30);
        const uint32_t pool = poolRef ? Read32(base, poolRef) : 0;
        fprintf(stderr, "[speedbreaker] %s input=%.3f stored=%.3f base_mass=%.3f multiplier=%.3f\n",
            activation ? "activate" : "deactivate", original, ReadFloat(base, object + 0x3C),
            pool ? ReadFloat(base, pool + 0x2C) : 0, s_multiplier);
    }
}
