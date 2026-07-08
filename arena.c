#include "mirage.h"
#include <windows.h>

internal char *base;
internal u64 arena_size = MB(64);
internal u64 arena_pos;

void *arena_push(u64 count, u64 size)
{
    u64 need = count * size;
    if (arena_pos + need > arena_size || !base) {
        arena_size = need > arena_size ? need * 2 : arena_size;
        base = VirtualAlloc(0, arena_size, MEM_COMMIT, PAGE_READWRITE);
        arena_pos = need;
        return base;
    }

    void *result = base + arena_pos;
    arena_pos += need;
    return result;
}

void *arena_realloc(void *buf, u64 before_size, u64 after_size)
{
    u64 difference = after_size - before_size;
    if ((buf == base + (arena_pos - before_size)) && ((arena_pos + difference) <= arena_size)) {
        arena_pos = arena_pos + difference;
        return buf;
    }

    void *result = arena_push(1, after_size);
    memcpy(result, buf, before_size);
    return result;
}
