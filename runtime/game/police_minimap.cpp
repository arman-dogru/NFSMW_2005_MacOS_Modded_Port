// SpeedBreaker. GPL-3.0-or-later (see COPYING).
#include <stdafx.h>
#include <kernel/function.h>
#include <region/region.h>
#include <user/settings.h>

extern "C" PPC_FUNC(__imp__sub_8229B840);
extern "C" PPC_FUNC(__imp__sub_8229AD08);

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
    bool Pointer(uint32_t p, uint32_t extent = 0x100)
    {
        return !(p & 3) && p >= 0x10000 && uint64_t(p) + extent < 0x100000000ull;
    }
    const bool s_log = [] {
        const char* v = std::getenv("NFSMW_POLICE_SPAWN_LOG");
        return v && std::strcmp(v, "1") == 0;
    }();
    struct Art
    {
        uint32_t original = 0;
        std::array<uint32_t, 56> copies{}; // 8 stock + 56 = original vehicle ceiling.
        unsigned count = 0;
        bool unavailable = false;
    };
    std::unordered_map<uint32_t, Art> s_art;
    bool Unavailable(Art& art, const char* reason)
    {
        art.unavailable = true;
        if (s_log) fprintf(stderr, "[police-minimap] extra icons unavailable: %s\n", reason);
        return false;
    }
    // USA FEPackage traversal (82302538) starts at +4C. FEGroup traversal
    // (823024A8) starts at +64. Their FEMinList headers start eight bytes
    // earlier. Verify the count, tail and sibling links before extending one.
    uint32_t ParentList(uint8_t* base, uint32_t list, uint32_t object, unsigned depth = 0)
    {
        if (depth > 16 || !Pointer(list, 16)) return 0;
        const uint32_t count = Read32(base, list + 4);
        if (!count || count > 2048) return 0;
        uint32_t previous = 0, node = Read32(base, list + 8), found = 0;
        for (uint32_t i = 0; i < count; ++i)
        {
            if (!Pointer(node, 0x6C) || Read32(base, node + 8) != previous) return 0;
            if (node == object) found = list;
            else if (!found && Read32(base, node + 0x18) == 5)
                found = ParentList(base, node + 0x5C, object, depth + 1);
            previous = node;
            node = Read32(base, node + 4);
        }
        return !node && previous == Read32(base, list + 12) ? found : 0;
    }
    void DestroyCopy(uint8_t* base, uint32_t copy)
    {
        GuestToHostFunction<void>(Read32(base, Read32(base, copy)), copy, 1u);
    }
    bool Grow(uint8_t* base, uint32_t minimap, Art& art, unsigned wanted)
    {
        if (art.unavailable || art.count >= wanted) return false;
        const uint32_t original = art.original;
        // Live readback and Clone body 822FC150 independently confirm FEImage.
        if (Read32(base, original) != 0x82080060 || Read32(base, original + 0x18) != 1)
            return Unavailable(art, "unexpected FEImage");
        const uint32_t feng = Read32(base, 0x82A2C5C4), name = Read32(base, minimap + 0x10);
        const uint32_t package = feng && name ? GuestToHostFunction<uint32_t>(0x82284CA8u, feng, name) : 0;
        const uint32_t parent = Pointer(package, 0x54) ? ParentList(base, package + 0x44, original) : 0;
        const uint32_t pool = Read32(base, 0x82A2BF04);
        if (!parent || !Pointer(pool, 0x2C)) return Unavailable(art, "package/list validation");
        const uint32_t source = Read32(base, original + 0x2C), size = Read32(base, original + 0x30);
        if (!size || size > 4096 || !Pointer(source, size)) return Unavailable(art, "image data validation");

        // At most two allocations per update. Clones belong to the FE package,
        // and the original minimap destructor returns tracking nodes to its
        // pool. No host pointer is stored in guest memory.
        const unsigned end = std::min(wanted, art.count + 2);
        while (art.count < end)
        {
            const uint32_t copy = GuestToHostFunction<uint32_t>(0x822FC150u, original, 0u);
            if (!copy) return Unavailable(art, "image allocation");
            const uint32_t data = Read32(base, copy + 0x2C);
            if (Read32(base, copy) != 0x82080060 || Read32(base, copy + 0x30) != size || !Pointer(data, size))
            {
                DestroyCopy(base, copy);
                return Unavailable(art, "cloned image validation");
            }
            // Clone's base copy constructor 82300CE8 allocates/zeros pData;
            // copy this image's current art, not its render cache or links.
            std::memcpy(base + data, base + source, size);
            Write32(base, copy + 0x58, 0);
            Write32(base, copy + 0x10, Read32(base, original + 0x10));
            const uint32_t tracking = GuestToHostFunction<uint32_t>(0x82159FD0u, pool);
            if (!tracking)
            {
                DestroyCopy(base, copy);
                return Unavailable(art, "tracking node allocation");
            }
            const uint32_t previous = art.count ? art.copies[art.count - 1] : original;
            const uint32_t next = Read32(base, previous + 4);
            Write32(base, copy + 4, next);
            Write32(base, copy + 8, previous);
            if (next) Write32(base, next + 8, copy);
            else Write32(base, parent + 12, copy);
            Write32(base, previous + 4, copy);
            Write32(base, parent + 4, Read32(base, parent + 4) + 1);

            const uint32_t sentinel = minimap + 8, tail = Read32(base, sentinel + 4);
            Write32(base, tracking + 8, copy);
            Write32(base, tracking, sentinel);
            Write32(base, tracking + 4, tail);
            Write32(base, tail, tracking);
            Write32(base, sentinel + 4, tracking);
            GuestToHostFunction<void>(0x8227FE08u, copy);
            art.copies[art.count++] = copy;
        }
        if (s_log) fprintf(stderr, "[police-minimap] icons=%u (8 stock, %u added)\n", 8 + art.count, art.count);
        return true;
    }
    void ExtraMarkers(uint8_t* base, uint32_t minimap, bool show)
    {
        const uint32_t original = Read32(base, minimap + 0xD0);
        if (!Pointer(original, 0x60)) return;
        auto found = s_art.find(minimap);
        if (!show && found == s_art.end()) return;
        Art& art = found != s_art.end() ? found->second : s_art[minimap];
        if (art.original != original) art = Art{.original = original};
        for (unsigned i = 0; i < art.count; ++i)
            GuestToHostFunction<void>(0x8227FE08u, art.copies[i]);
        if (!show) return;
        const uint32_t count = Read32(base, 0x82C6F314), vehicles = Read32(base, 0x82C6F30C);
        if (count > 96 || !Pointer(vehicles, count * 4)) return;
        // Prepare the usual doubled eight-car ceiling before a large group
        // arrives, spreading the small allocations over four HUD updates.
        Grow(base, minimap, art, std::max(8u, count > 8 ? std::min(count - 8, 56u) : 0u));
        if (!art.count) return;
        static thread_local uint32_t vectors = [] {
            auto* p = g_userHeap.Alloc(16);
            return p ? g_memory.MapVirtual(p) : 0u;
        }();
        if (!vectors) return;
        unsigned ground = 0;
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t vehicle = Read32(base, vehicles + i * 4);
            if (!Pointer(vehicle) || Read32(base, vehicle) != 0x8208AD18) continue;
            const uint32_t vt = Read32(base, vehicle);
            if (!GuestToHostFunction<uint32_t>(Read32(base, vt + 0x88), vehicle)) continue;
            const uint32_t attributes = GuestToHostFunction<uint32_t>(Read32(base, vt + 0x44), vehicle);
            // Original updater keeps helicopter art separate from ground cars.
            if (!Pointer(attributes, 4) || Read32(base, attributes) == Read32(base, 0x82A3E330)) continue;
            if (ground++ < 8) continue;
            const unsigned index = ground - 9;
            if (index >= art.count) break;
            const uint32_t icon = art.copies[index];
            const uint32_t simable = GuestToHostFunction<uint32_t>(Read32(base, vt + 4), vehicle);
            GuestToHostFunction<void>(0x8229A030u, vectors, vectors + 8, simable);
            uint32_t colour = 0xFFCCCCCC;
            const uint32_t ai = Read32(base, vehicle + 0x54);
            if (Pointer(ai) && Read32(base, ai) == 0x82065A78 &&
                GuestToHostFunction<uint32_t>(Read32(base, Read32(base, ai) + 0x14), ai))
            {
                const uint32_t phase = Read32(base, minimap + 0xC0);
                if (phase <= 2) colour = 0xFF0000FF;
                else if (phase >= 4 && phase <= 6) colour = 0xFFA00000;
            }
            GuestToHostFunction<void>(0x822819B0u, icon, colour);
            GuestToHostFunction<void>(0x8229B5F0u, minimap, vectors, vectors + 8, icon, 0u);
        }
        static unsigned lastGround = UINT_MAX;
        if (s_log && ground != lastGround)
            fprintf(stderr, "[police-minimap] active_ground=%u extra_markers=%u\n",
                ground, ground > 8 ? std::min(ground - 8, art.count) : 0u);
        lastGround = ground;
    }
}

// Hud_Minimap's police update. The byte bypasses the player-pursuit gate
// (8229B89C) and per-cop pursuit visibility gates (8229B9A8/8229BAEC).
// Preserve validity, positions, original art and minimap clipping.
PPC_FUNC(sub_8229B840)
{
    if (region::Current() != region::Release::Usa) { __imp__sub_8229B840(ctx, base); return; }
    const uint32_t minimap = ctx.r3.u32;
    const bool show = settings::GetBool(settings::Id::ShowAllPolice);
    struct RestoreByte
    {
        uint8_t& value;
        uint8_t saved;
        ~RestoreByte() { value = saved; }
    } restore{base[0x82A2C5FA], base[0x82A2C5FA]};
    if (show) base[0x82A2C5FA] = 1;
    __imp__sub_8229B840(ctx, base);
    ExtraMarkers(base, minimap, show);
}

PPC_FUNC(sub_8229AD08)
{
    if (region::Current() == region::Release::Usa) s_art.erase(ctx.r3.u32);
    __imp__sub_8229AD08(ctx, base);
}
