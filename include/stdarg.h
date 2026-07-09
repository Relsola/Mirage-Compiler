#ifndef __STDARG_H
#define __STDARG_H

typedef char *va_list;

#define va_start(ap, last) \
    do {                   \
        ap = __va_area__;  \
    } while (0)

#define va_arg(ap, type)     \
    ({                       \
        ap += 8;             \
        *((type *)(ap - 8)); \
    })

#define va_copy(dest, src) \
    do {                   \
        dest = src;        \
    } while (0)

#define va_end(ap)

#endif
