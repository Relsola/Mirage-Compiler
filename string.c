#include "mirage.h"

void strarray_push(StringArray *arr, char *s)
{
    if (!arr->data) {
        arr->data = arena_push(8, sizeof(char *));
        arr->capacity = 8;
    }

    if (arr->capacity == arr->len) {
        arr->data = arena_realloc(arr->data, sizeof(char *) * arr->capacity, sizeof(char *) * arr->capacity * 2);
        arr->capacity *= 2;
        for (int i = arr->len; i < arr->capacity; i++) {
            arr->data[i] = nullptr;
        }
    }

    arr->data[arr->len++] = s;
}

char *strndup(const char *source, u32 size)
{
    char *buf = arena_push(1, size + 1);
    memcpy(buf, source, size);
    return buf;
}

// Takes a printf-style format string and returns a formatted string.
char *format(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    va_list ap2;
    va_copy(ap2, ap);

    int len = _vscprintf(fmt, ap);

    char *buf = arena_push(1, len + 1);
    vsnprintf(buf, (size_t)len + 1, fmt, ap2);

    va_end(ap);
    va_end(ap2);
    return buf;
}
