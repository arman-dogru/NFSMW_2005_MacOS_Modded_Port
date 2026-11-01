// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
// Read-only USA cop-manager mapping diagnostics. NFSMW_POLICE_PROBE=1.
#include <stdafx.h>
#include <region/region.h>
#include <report/report.h>
#include "police_probe.h"

extern "C" PPC_FUNC(__imp__sub_820F2320);
extern "C" PPC_FUNC(__imp__sub_820EB970);
extern "C" PPC_FUNC(__imp__sub_8211BBF0);

namespace
{
    const bool s_enabled = [] {
        const char* v = std::getenv("NFSMW_POLICE_PROBE");
        return v && std::strcmp(v, "1") == 0;
    }();

    uint32_t Read32(const uint8_t* base, uint32_t addr)
    {
        uint32_t v;
        std::memcpy(&v, base + addr, sizeof(v));
        return ByteSwap(v);
    }

    void Observe(uint8_t* base, uint32_t manager)
    {
        // These fields are read by the original routine just before this
        // probe, on the same guest thread. Do not follow arbitrary offsets.
        const uint32_t data = Read32(base, manager + 0xC8);
        const uint32_t count = Read32(base, manager + 0xD0);
        static double last = -10;
        static uint32_t lastCount = UINT32_MAX;
        const double now = report::SessionSeconds();
        if (count == lastCount && now - last < 5)
            return;
        last = now;
        lastCount = count;
        fprintf(stderr, "[police-probe] t=%.2f manager=%08X vtable=%08X vehicles=%u data=%08X\n",
            now, manager, Read32(base, manager), count, data);
        // The original uses this same array, not the PC global vehicle list.
        // Guard the arithmetic and cap diagnostic work even on bad state.
        if (count > 96 || (!data && count) || (data & 3) || uint64_t(data) + count * 4ull > UINT32_MAX)
            return;
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t vehicle = Read32(base, data + i * 4);
            if (!vehicle || (vehicle & 3) || vehicle > UINT32_MAX - 4)
                continue;
            const uint32_t ai = Read32(base, vehicle + 0x54);
            fprintf(stderr, "[police-probe]   vehicle[%u]=%08X vtable=%08X driver=%u ai=%08X ai_vtable=%08X\n",
                i, vehicle, Read32(base, vehicle), Read32(base, vehicle + 0x9C), ai,
                ai && !(ai & 3) ? Read32(base, ai) : 0);
            if (ai && !(ai & 3) && Read32(base, ai) == 0x82065A78)
            {
                // Getter bodies: 0x820E90D0, 0x820E90F8, 0x820E9128.
                const uint32_t target = Read32(base, ai + 0x68);
                fprintf(stderr, "[police-probe]     goal=%08X pursuit=%08X target=%08X target_vt=%08X simable=%08X valid=%u\n",
                    Read32(base, ai + 0x8C), Read32(base, ai + 0x84), target,
                    target ? Read32(base, target) : 0,
                    target ? Read32(base, target + 0x2C) : 0,
                    target ? unsigned(base[target + 0x3C]) : 0);
            }
        }
        // This global is independently used as an array by 0x820EC060.
        // IVehicle element type was confirmed live in Circuit/Sprint tests.
        const uint32_t objects = Read32(base, 0x82C6F18C);
        const uint32_t objectCount = Read32(base, 0x82C6F194);
        fprintf(stderr, "[police-probe]   global count=%u data=%08X\n", objectCount, objects);
        if (objectCount > 256 || !objects || (objects & 3))
            return;
        for (uint32_t i = 0; i < std::min(objectCount, 24u); ++i)
        {
            const uint32_t object = Read32(base, objects + i * 4);
            if (object && !(object & 3))
            {
                const uint32_t vt = Read32(base, object);
                fprintf(stderr, "[police-probe]   object[%u]=%08X vtable=%08X", i, object, vt);
                if (vt == 0x8208AD18)
                {
                    const uint32_t ai = Read32(base, object + 0x54);
                    fprintf(stderr, " driver=%u ai=%08X ai_vtable=%08X", Read32(base, object + 0x9C),
                        ai, ai ? Read32(base, ai) : 0);
                    const uint32_t body = Read32(base, object - 0x40);
                    fprintf(stderr, " body=%08X body_vtable=%08X", body, body ? Read32(base, body) : 0);
                }
                fprintf(stderr, "\n");
            }
        }
    }
}

PPC_FUNC(sub_820EB970)
{
    const uint32_t manager = ctx.r3.u32, model = ctx.r4.u32;
    __imp__sub_820EB970(ctx, base);
    if (!s_enabled || region::Current() != region::Release::Usa) return;
    static double last = -10;
    const double now = report::SessionSeconds();
    if (!ctx.r3.u32 && now - last < 5) return;
    last = now;
    fprintf(stderr, "[police-probe] create model=%.32s result=%08X manager_count=%u\n",
        model ? reinterpret_cast<const char*>(base + model) : "(null)", ctx.r3.u32, Read32(base, manager + 0xD0));
}

PPC_FUNC(sub_8211BBF0)
{
    __imp__sub_8211BBF0(ctx, base);
    if (!s_enabled || region::Current() != region::Release::Usa) return;
    static double last = -10;
    const double now = report::SessionSeconds();
    if (now - last < 5) return;
    last = now;
    fprintf(stderr, "[police-probe] patrol_spawn_point=%u\n", ctx.r3.u32);
}

void game::police_probe::ObserveCanSpawn(uint8_t* base, uint32_t managerInterface, uint32_t arg, uint32_t result)
{
    if (!s_enabled || region::Current() != region::Release::Usa) return;
    static double last = -10;
    const double now = report::SessionSeconds();
    if (now - last < 5) return;
    last = now;
    uint32_t bits = Read32(base, managerInterface + 0x58);
    float cooldown;
    std::memcpy(&cooldown, &bits, 4);
    const uint32_t race = Read32(base, 0x82A2CB18);
    fprintf(stderr, "[police-probe] can_spawn=%u arg=%u block=%u cooldown=%.2f global_block=%u script=%08X race=%08X state=%u params=%08X manager70=%u\n",
        result, arg, unsigned(base[managerInterface + 0x60]), cooldown,
        Read32(base, 0x82A2BD94), Read32(base, 0x82C5E750), race,
        race ? Read32(base, race + 0x1A28) : 0, race ? Read32(base, race + 0x1A30) : 0,
        Read32(base, managerInterface + 0x1C));
}

// Observe after the complete task. At entry r3 is the manager's +8 interface.
// This wrapper performs no gameplay writes and creates no guest objects.
PPC_FUNC(sub_820F2320)
{
    const uint32_t manager = ctx.r3.u32 - 8;
    __imp__sub_820F2320(ctx, base);
    if (s_enabled && region::Current() == region::Release::Usa)
        Observe(base, manager);
}
