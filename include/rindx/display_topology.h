/* SPDX-License-Identifier: MIT */
#ifndef RINDX_PUBLIC_DISPLAY_TOPOLOGY_H
#define RINDX_PUBLIC_DISPLAY_TOPOLOGY_H

#include <stdint.h>

#define RINDX_DISPLAY_TOPOLOGY_VERSION UINT32_C(1)
#define RINDX_DISPLAY_TOPOLOGY_MAX UINT32_C(8)
#define RINDX_DISPLAY_FLAG_CONNECTED UINT32_C(0x00000001)
#define RINDX_DISPLAY_FLAG_PRIMARY UINT32_C(0x00000002)
#define RINDX_DISPLAY_FLAG_ENABLED UINT32_C(0x00000004)
#define RINDX_DISPLAY_FLAG_HDR UINT32_C(0x00000008)
#define RINDX_DISPLAY_FLAG_KNOWN \
    (RINDX_DISPLAY_FLAG_CONNECTED | RINDX_DISPLAY_FLAG_PRIMARY | \
     RINDX_DISPLAY_FLAG_ENABLED | RINDX_DISPLAY_FLAG_HDR)

typedef struct RinDxgiDisplayTargetV1 {
    uint32_t struct_size;
    uint32_t version;
    uint64_t target_id;
    uint64_t generation;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t scale_percent;
    uint32_t flags;
    uint32_t reserved0;
    uint32_t reserved1;
} RinDxgiDisplayTargetV1;

typedef struct RinDxgiDisplayTopologyV1 {
    uint32_t struct_size;
    uint32_t version;
    uint64_t generation;
    uint32_t target_count;
    uint32_t primary_index;
    int32_t virtual_x;
    int32_t virtual_y;
    uint32_t virtual_width;
    uint32_t virtual_height;
    uint32_t reserved0;
    RinDxgiDisplayTargetV1 targets[RINDX_DISPLAY_TOPOLOGY_MAX];
} RinDxgiDisplayTopologyV1;

static inline int rindx_display_target_valid(
    const RinDxgiDisplayTargetV1* target) {
    return target != NULL && target->struct_size == sizeof(*target) &&
           target->version == RINDX_DISPLAY_TOPOLOGY_VERSION &&
           target->target_id != 0u && target->generation != 0u &&
           target->width >= 320u && target->width <= 16384u &&
           target->height >= 200u && target->height <= 8640u &&
           target->scale_percent >= 50u && target->scale_percent <= 300u &&
           (target->flags & ~RINDX_DISPLAY_FLAG_KNOWN) == 0u &&
           (target->flags & RINDX_DISPLAY_FLAG_CONNECTED) != 0u &&
           target->reserved0 == 0u && target->reserved1 == 0u;
}

static inline int rindx_display_topology_valid(
    const RinDxgiDisplayTopologyV1* topology) {
    int64_t min_x = INT64_MAX;
    int64_t min_y = INT64_MAX;
    int64_t max_x = INT64_MIN;
    int64_t max_y = INT64_MIN;
    uint32_t primary_count = 0u;
    uint32_t index;

    if (topology == NULL || topology->struct_size != sizeof(*topology) ||
        topology->version != RINDX_DISPLAY_TOPOLOGY_VERSION ||
        topology->generation == 0u || topology->target_count == 0u ||
        topology->target_count > RINDX_DISPLAY_TOPOLOGY_MAX ||
        topology->primary_index >= topology->target_count ||
        topology->reserved0 != 0u)
        return 0;
    for (index = 0u; index < topology->target_count; ++index) {
        const RinDxgiDisplayTargetV1* target = &topology->targets[index];
        uint32_t other;
        int64_t right;
        int64_t bottom;
        if (!rindx_display_target_valid(target) ||
            target->generation != topology->generation)
            return 0;
        right = (int64_t)target->x + (int64_t)target->width;
        bottom = (int64_t)target->y + (int64_t)target->height;
        if (right > INT32_MAX || right < INT32_MIN ||
            bottom > INT32_MAX || bottom < INT32_MIN)
            return 0;
        if ((target->flags & RINDX_DISPLAY_FLAG_PRIMARY) != 0u) {
            ++primary_count;
            if (index != topology->primary_index ||
                (target->flags & RINDX_DISPLAY_FLAG_ENABLED) == 0u)
                return 0;
        }
        if ((target->flags & RINDX_DISPLAY_FLAG_ENABLED) != 0u) {
            if (target->x < min_x) min_x = target->x;
            if (target->y < min_y) min_y = target->y;
            if (right > max_x) max_x = right;
            if (bottom > max_y) max_y = bottom;
        }
        for (other = 0u; other < index; ++other) {
            const RinDxgiDisplayTargetV1* prior = &topology->targets[other];
            int64_t prior_right = (int64_t)prior->x + prior->width;
            int64_t prior_bottom = (int64_t)prior->y + prior->height;
            if (prior->target_id == target->target_id ||
                ((target->flags & RINDX_DISPLAY_FLAG_ENABLED) != 0u &&
                 (prior->flags & RINDX_DISPLAY_FLAG_ENABLED) != 0u &&
                 target->x < prior_right && prior->x < right &&
                 target->y < prior_bottom && prior->y < bottom))
                return 0;
        }
    }
    if (primary_count != 1u || min_x == INT64_MAX || min_y == INT64_MAX ||
        max_x <= min_x || max_y <= min_y ||
        topology->virtual_x != min_x || topology->virtual_y != min_y ||
        topology->virtual_width != (uint32_t)(max_x - min_x) ||
        topology->virtual_height != (uint32_t)(max_y - min_y))
        return 0;
    for (index = topology->target_count;
         index < RINDX_DISPLAY_TOPOLOGY_MAX; ++index) {
        const uint8_t* bytes = (const uint8_t*)&topology->targets[index];
        uint32_t byte;
        for (byte = 0u; byte < sizeof(topology->targets[index]); ++byte)
            if (bytes[byte] != 0u) return 0;
    }
    return 1;
}

#if defined(__cplusplus)
static_assert(sizeof(RinDxgiDisplayTargetV1) == 56u,
              "RinDX display target ABI drift");
static_assert(sizeof(RinDxgiDisplayTopologyV1) ==
                  (sizeof(uintptr_t) == 4u ? 492u : 496u),
              "RinDX display topology ABI drift");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(RinDxgiDisplayTargetV1) == 56u,
               "RinDX display target ABI drift");
_Static_assert(sizeof(RinDxgiDisplayTopologyV1) ==
                   (sizeof(uintptr_t) == 4u ? 492u : 496u),
               "RinDX display topology ABI drift");
#endif

#endif /* RINDX_PUBLIC_DISPLAY_TOPOLOGY_H */
