#include "whwd/whwd.h"

#include <stdlib.h>
#include <string.h>

#include "sources/source.h"

extern const whwd_source whwd_wu_source;
extern const whwd_source whwd_realtek_source;
extern const whwd_source whwd_intel_source;

const whwd_source *const whwd_sources[] = {
    &whwd_wu_source,
    &whwd_realtek_source,
    &whwd_intel_source,
};

const size_t whwd_sources_count = sizeof(whwd_sources) / sizeof(whwd_sources[0]);

const char *whwd_source_name(size_t index)
{
    if (index >= whwd_sources_count) return NULL;
    return whwd_sources[index]->name;
}

size_t whwd_source_count(void)
{
    return whwd_source_count;
}

int whwd_check_updates(const whwd_features *features, whwd_update **out, size_t *count)
{
    if (!out || !count) return -1;
    *out = NULL;
    *count = 0;

    whwd_update *all = NULL;
    size_t n = 0;

    for (size_t i = 0; i < whwd_sources_count; i++) {
        const whwd_source *src = whwd_sources[i];
        whwd_source_status st = src->status(features);
        if (st != WHWD_SOURCE_OK) continue;

        whwd_update *found = NULL;
        size_t found_count = 0;
        if (src->check(src, features, &found, &found_count) != 0 || found_count == 0)
            continue;

        whwd_update *grown = (whwd_update *)realloc(all, (n + found_count) * sizeof(whwd_update));
        if (!grown) {
            free(found);
            break;
        }
        all = grown;
        memcpy(all + n, found, found_count * sizeof(whwd_update));
        n += found_count;
        free(found);
    }

    *out = all;
    *count = n;
    return 0;
}

void whwd_free_updates(whwd_update *updates)
{
    free(updates);
}