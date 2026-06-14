#include "mirage.h"

String *new_string(char *start, char *end)
{
    String *str = calloc(1, sizeof(String));
    str->loc = start;
    str->len = end - start;
    return str;
}

bool string_equal(String *s1, String *s2)
{
    if (s1->len == s2->len && s1->loc == s2->loc) {
        return true;
    }

    bool result = s1->len == s2->len && !strncmp(s1->loc, s2->loc, s1->len);
    return result;
}

char *to_c_str(String *s)
{
    if (s->c_str) {
        return s->c_str;
    }

    char *buf = malloc((s->len + 1) * sizeof(char));
    memcpy(buf, s->loc, s->len);
    buf[s->len] = '\0';
    s->c_str = buf;
    return s->c_str;
}
