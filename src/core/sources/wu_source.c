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

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    int com_initialized = SUCCEEDED(hr);
    if (!com_initialized && hr != RPC_E_CHANGED_MODE) return 0;

    IUpdateSession *session = NULL;
    IUpdateSearcher *searcher = NULL;
    BSTR criteria = NULL;
    ISearchResult *result = NULL;
    IUpdateCollection *updates = NULL;
    whwd_update *arr = NULL;
    size_t n = 0;

    hr = CoCreateInstance(&CLSID_UpdateSession, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IUpdateSession, (void **)&session);
    if (FAILED(hr)) goto done;

    hr = session->lpVtbl->CreateUpdateSearcher(session, &searcher);
    if (FAILED(hr)) goto done;

    criteria = SysAllocString(L"IsInstalled=0 and Type='Driver'");
    if (!criteria) goto done;

    hr = searcher->lpVtbl->Search(searcher, criteria, &result);
    if (FAILED(hr)) goto done;

    hr = result->lpVtbl->get_Updates(result, &updates);
    if (FAILED(hr) || !updates) goto done;

    long total = 0;
    if (FAILED(updates->lpVtbl->get_Count(updates, &total))) total = 0;
    if (total > WU_MAX_UPDATES) total = WU_MAX_UPDATES;

    if (total > 0) {
        arr = (whwd_update *)calloc((size_t)total, sizeof(whwd_update));
        if (arr) {
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
        }
    }

done:
    if (criteria) SysFreeString(criteria);
    if (searcher) searcher->lpVtbl->Release(searcher);
    if (session) session->lpVtbl->Release(session);
    if (result) result->lpVtbl->Release(result);
    if (updates) updates->lpVtbl->Release(updates);

    *out = arr;
    *count = n;

    if (com_initialized) CoUninitialize();
    return 0;
}

const whwd_source whwd_wu_source = {
    "windows-update",
    "Windows Update",
    wu_status,
    wu_check,
};