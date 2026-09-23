#ifndef WHWD_SOURCE_H
#define WHWD_SOURCE_H

#include "whwd/whwd.h"

typedef struct whwd_source whwd_source;

struct whwd_source {
    const char *name;
    const char *label;
    whwd_source_status (*status)(const whwd_features *features);
    int (*check)(const whwd_source *self, const whwd_features *features,
                 whwd_update **out, size_t *count);
};

extern const whwd_source *const whwd_sources[];
extern const size_t whwd_sources_count;

#endif