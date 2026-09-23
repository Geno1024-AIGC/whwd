#include "whwd/whwd.h"

#include <string.h>

#include <windows.h>
#include <winternl.h>

#include <wuapi.h>

typedef LONG (NTAPI *rtl_getversion_fn)(PRTL_OSVERSIONINFOW);

static void detect_os(whwd_features *f)
{
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return;
    rtl_getversion_fn fn = (rtl_getversion_fn)GetProcAddress(ntdll, "RtlGetVersion");
    if (!fn) return;

    RTL_OSVERSIONINFOW vi;
    memset(&vi, 0, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(vi);
    if (fn(&vi) != 0) return;

    f->os_major = (int)vi.dwMajorVersion;
    f->os_minor = (int)vi.dwMinorVersion;
    f->os_build = (int)vi.dwBuildNumber;
}

static int detect_wua(void)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    int initialized = SUCCEEDED(hr);
    if (!initialized && hr != RPC_E_CHANGED_MODE) return 0;

    IUpdateSession *sess = NULL;
    hr = CoCreateInstance(&CLSID_UpdateSession, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IUpdateSession, (void **)&sess);
    if (sess) sess->lpVtbl->Release(sess);

    if (initialized) CoUninitialize();
    return SUCCEEDED(hr) ? 1 : 0;
}

static int detect_pnputil(void)
{
    return GetFileAttributesW(L"C:\\Windows\\System32\\pnputil.exe") != INVALID_FILE_ATTRIBUTES;
}

void whwd_detect_features(whwd_features *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));

    detect_os(out);
    out->wua = detect_wua();
    out->pnputil = detect_pnputil();
}