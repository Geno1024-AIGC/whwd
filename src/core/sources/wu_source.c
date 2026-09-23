#include "whwd/whwd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <wuapi.h>

#include "source.h"

#define WU_MAX_UPDATES 256

static whwd_source_status wu_status(const whwd_features *features)
{
    if (!features) return WHWD_SOURCE_UNAVAILABLE;
    if (!features->wua) return WHWD_SOURCE_UNAVAILABLE;
    if (features->os_major < 6) return WHWD_SOURCE_UNSUPPORTED_OS;
    return WHWD_SOURCE_OK;
}

static int wu_check(const struct whwd_source *self, const whwd_features *features,
                    whwd_update **out, size_t *count)
{
    if (out) *out = NULL;
    if (count) *count = 0;
    if (!out || !count || !features || !features->wua) return 0;

    HRESULT hr;
    IUpdateSession *session = NULL;
    hr = CoCreateInstance(&CLSID_UpdateSession, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IUpdateSession, (void **)&session);
    if (FAILED(hr)) return 0;

    IUpdateSearcher *searcher = NULL;
    hr = session->lpVtbl->CreateUpdateSearcher(session, &searcher);
    session->lpVtbl->Release(session);
    if (FAILED(hr)) return 0;

    BSTR criteria = SysAllocString(L"IsInstalled=0 and Type='Driver'");
    if (!criteria) {
        searcher->lpVtbl->Release(searcher);
        return 0;
    }

    ISearchResult *result = NULL;
    hr = searcher->lpVtbl->Search(searcher, criteria, &result);
    SysFreeString(criteria);
    searcher->lpVtbl->Release(searcher);
    if (FAILED(hr)) return 0;

    IUpdateCollection *updates = NULL;
    hr = result->lpVtbl->get_Updates(result, &updates);
    result->lpVtbl->Release(result);
    if (FAILED(hr)) return 0;

    long total = 0;
    if (FAILED(updates->lpVtbl->get_Count(updates, &total))) total = 0;
    if (total > WU_MAX_UPDATES) total = WU_MAX_UPDATES;

    if (total > 0) {
        whwd_update *arr = (whwd_update *)calloc((size_t)total, sizeof(whwd_update));
        size_t n = 0;

        for (long i = 0; i < total; i++) {
            IUpdate *item = NULL;
            if (FAILED(updates->lpVtbl->get_Item(updates, i, &item))) continue;

            BSTR title = NULL;
            hr = item->lpVtbl->get_Title(item, &title);
            item->lpVtbl->Release(item);
            if (FAILED(hr) || !title) continue;

            whwd_update *u = &arr[n];
            snprintf(u->id, sizeof(u->id), "wu%ld", i);
            memcpy(u->source, self->name, strlen(self->name) + 1);
            WideCharToMultiByte(CP_UTF8, 0, title, -1,
                                u->title, (int)sizeof(u->title), NULL, NULL);
            SysFreeString(title);
            n++;
        }

        if (n == 0) {
            free(arr);
            arr = NULL;
        }
        *out = arr;
        *count = n;
    }

    updates->lpVtbl->Release(updates);
    return 0;
}

const whwd_source whwd_wu_source = {
    "windows-update",
    "Windows Update",
    wu_status,
    wu_check,
};