#include "whwd/whwd.h"

#include <ctype.h>

static int eat_number(const char **cursor)
{
    int value = 0;
    const char *s = *cursor;
    while (*s && isdigit((unsigned char)*s)) {
        value = value * 10 + (*s - '0');
        s++;
    }
    *cursor = s;
    return value;
}

int whwd_vercmp(const char *a, const char *b)
{
    if (!a) a = "";
    if (!b) b = "";

    while (*a || *b) {
        while (*a && !isdigit((unsigned char)*a)) a++;
        while (*b && !isdigit((unsigned char)*b)) b++;

        if (*a && *b) {
            int na = eat_number(&a);
            int nb = eat_number(&b);
            if (na != nb) return na < nb ? -1 : 1;
            continue;
        }
        break;
    }

    if (*a) return 1;
    if (*b) return -1;
    return 0;
}