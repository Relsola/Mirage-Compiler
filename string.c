#include "mirage.h"

void strarray_push(StringArray *arr, char *s)
{
    if (!arr->data) {
        arr->data = calloc(8, sizeof(char *));
        arr->capacity = 8;
    }

    if (arr->capacity == arr->len) {
        arr->data = realloc(arr->data, sizeof(char *) * arr->capacity * 2);
        arr->capacity *= 2;
        for (int i = arr->len; i < arr->capacity; i++) {
            arr->data[i] = NULL;
        }
    }

    arr->data[arr->len++] = s;
}

char *strndup(const char *source, u32 size)
{
    char *buf = malloc((size + 1) * sizeof(char));
    memcpy(buf, source, size);
    buf[size] = '\0';
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

    char *buf = malloc((size_t)len + 1);
    vsnprintf(buf, (size_t)len + 1, fmt, ap2);

    va_end(ap);
    va_end(ap2);
    return buf;
}
