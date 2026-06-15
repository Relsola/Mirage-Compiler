#include "mirage.h"

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
