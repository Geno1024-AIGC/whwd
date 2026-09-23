#include "whwd/whwd.h"

#include "source.h"

static whwd_source_status realtek_status(const whwd_features *features)
{
    (void)features;
    return WHWD_SOURCE_DISABLED;
}

static int realtek_check(const struct whwd_source *self, const whwd_features *features,
                         whwd_update **out, size_t *count)
{
    (void)self;
    (void)features;
    if (out) *out = NULL;
    if (count) *count = 0;
    return 0;
}

const whwd_source whwd_realtek_source = {
    "realtek",
    "Realtek (audio and network)",
    realtek_status,
    realtek_check,
};