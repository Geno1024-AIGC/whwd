#include "whwd/whwd.h"

#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <objbase.h>
#include <setupapi.h>

static int wstr_copy_utf8(const WCHAR *in, char *out, size_t outsz)
{
    if (!in || !out || outsz == 0 || outsz > INT_MAX) return -1;
    int n = WideCharToMultiByte(CP_UTF8, 0, in, -1, out, (int)outsz, NULL, NULL);
    return n > 0 ? 0 : -1;
}

static int query_reg_string(HKEY root, const WCHAR *subkey, const WCHAR *name,
                            char *out, size_t outsz)
{
    if (!out || outsz == 0) return -1;
    HKEY key = NULL;
    if (RegOpenKeyExW(root, subkey, 0, KEY_READ, &key) != ERROR_SUCCESS) return -1;

    DWORD type = 0, size = 0;
    LONG r = RegQueryValueExW(key, name, NULL, &type, NULL, &size);
    if (r != ERROR_SUCCESS || type != REG_SZ || size == 0) {
        RegCloseKey(key);
        return -1;
    }

    DWORD chars = (size + sizeof(WCHAR) - 1) / sizeof(WCHAR) + 1;
    WCHAR *tmp = (WCHAR *)malloc(chars * sizeof(WCHAR));
    if (!tmp) {
        RegCloseKey(key);
        return -1;
    }

    DWORD got = chars * sizeof(WCHAR);
    r = RegQueryValueExW(key, name, NULL, &type, (LPBYTE)tmp, &got);
    RegCloseKey(key);
    if (r != ERROR_SUCCESS || got == 0) {
        free(tmp);
        return -1;
    }

    DWORD got_chars = (got + sizeof(WCHAR) - 1) / sizeof(WCHAR);
    tmp[got_chars] = 0;
    int ok = wstr_copy_utf8(tmp, out, outsz);
    free(tmp);
    return ok;
}

static void read_prop(HDEVINFO devs, SP_DEVINFO_DATA *did, DWORD prop,
                      char *out, size_t outsz)
{
    if (!out || outsz == 0) return;
    out[0] = 0;

    DWORD type = 0, size = 0;
    if (!SetupDiGetDeviceRegistryPropertyW(devs, did, prop, &type, NULL, 0, &size) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) return;
    if (type != REG_SZ || size == 0) return;

    DWORD chars = (size + sizeof(WCHAR) - 1) / sizeof(WCHAR) + 1;
    WCHAR *tmp = (WCHAR *)malloc(chars * sizeof(WCHAR));
    if (!tmp) return;

    DWORD got = chars * sizeof(WCHAR);
    if (!SetupDiGetDeviceRegistryPropertyW(devs, did, prop, &type, (BYTE *)tmp, got, &got) ||
        got == 0) {
        free(tmp);
        return;
    }

    DWORD got_chars = (got + sizeof(WCHAR) - 1) / sizeof(WCHAR);
    tmp[got_chars] = 0;
    wstr_copy_utf8(tmp, out, outsz);
    free(tmp);
}

static void read_device_id(HDEVINFO devs, SP_DEVINFO_DATA *did, char *out, size_t outsz)
{
    if (!out || outsz == 0) return;
    out[0] = 0;

    DWORD type = 0, size = 0;
    if (!SetupDiGetDeviceRegistryPropertyW(devs, did, SPDRP_HARDWAREID, &type, NULL, 0, &size) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) return;
    if (type != REG_MULTI_SZ || size == 0) return;

    DWORD chars = (size + sizeof(WCHAR) - 1) / sizeof(WCHAR) + 1;
    WCHAR *tmp = (WCHAR *)malloc(chars * sizeof(WCHAR));
    if (!tmp) return;

    DWORD got = chars * sizeof(WCHAR);
    if (!SetupDiGetDeviceRegistryPropertyW(devs, did, SPDRP_HARDWAREID, &type, (BYTE *)tmp, got, &got) ||
        got == 0) {
        free(tmp);
        return;
    }

    DWORD got_chars = (got + sizeof(WCHAR) - 1) / sizeof(WCHAR);
    tmp[got_chars] = 0;
    wstr_copy_utf8(tmp, out, outsz);
    free(tmp);
}

static void read_class(HDEVINFO devs, SP_DEVINFO_DATA *did, char *out, size_t outsz)
{
    if (!out || outsz == 0) return;
    out[0] = 0;

    char guid_str[WHWD_CLASS_MAX] = {0};
    read_prop(devs, did, SPDRP_CLASSGUID, guid_str, sizeof(guid_str));
    if (!guid_str[0]) return;

    WCHAR wguid[WHWD_CLASS_MAX];
    if (MultiByteToWideChar(CP_UTF8, 0, guid_str, -1, wguid, WHWD_CLASS_MAX) <= 0)
        return;

    GUID guid;
    if (CLSIDFromString(wguid, &guid) != S_OK) return;

    WCHAR wclass[WHWD_CLASS_MAX];
    DWORD wclass_size = WHWD_CLASS_MAX;
    if (!SetupDiGetClassDescriptionW(&guid, wclass, wclass_size, &wclass_size)) return;

    wstr_copy_utf8(wclass, out, outsz);
}

static void read_driver_registry(const char *driver_path, whwd_device *dev)
{
    if (!driver_path || !driver_path[0]) return;

    size_t path_len = strlen(driver_path);
    if (path_len >= 300) return;

    char prefix[] = "SYSTEM\\CurrentControlSet\\Control\\Class\\";
    char full[480];
    memcpy(full, prefix, sizeof(prefix) - 1);
    memcpy(full + sizeof(prefix) - 1, driver_path, path_len);
    full[sizeof(prefix) - 1 + path_len] = 0;

    int wlen = MultiByteToWideChar(CP_UTF8, 0, full, -1, NULL, 0);
    if (wlen <= 0) return;
    WCHAR *wpath = (WCHAR *)malloc((size_t)wlen * sizeof(WCHAR));
    if (!wpath) return;
    MultiByteToWideChar(CP_UTF8, 0, full, -1, wpath, wlen);

    query_reg_string(HKEY_LOCAL_MACHINE, wpath, L"DriverVersion",
                     dev->driver_version, sizeof(dev->driver_version));
    query_reg_string(HKEY_LOCAL_MACHINE, wpath, L"DriverDate",
                     dev->driver_date, sizeof(dev->driver_date));
    query_reg_string(HKEY_LOCAL_MACHINE, wpath, L"ProviderName",
                     dev->driver_provider, sizeof(dev->driver_provider));

    free(wpath);
}

int whwd_list_devices(whwd_device **out, size_t *count)
{
    if (!out || !count) return -1;
    *out = NULL;
    *count = 0;

    static const GUID *const k_driver_classes[] = {
        &GUID_DEVCLASS_DISPLAY,
        &GUID_DEVCLASS_MEDIA,
        &GUID_DEVCLASS_NET,
        &GUID_DEVCLASS_BLUETOOTH,
        &GUID_DEVCLASS_SCSIADAPTER,
        &GUID_DEVCLASS_HDC,
        &GUID_DEVCLASS_USB,
        &GUID_DEVCLASS_SYSTEM,
        &GUID_DEVCLASS_MOUSE,
        &GUID_DEVCLASS_KEYBOARD,
    };

    whwd_device *arr = NULL;
    size_t n = 0;

    for (size_t c = 0;
         c < sizeof(k_driver_classes) / sizeof(k_driver_classes[0]); c++) {
        HDEVINFO devs = SetupDiGetClassDevsW(k_driver_classes[c], NULL, NULL,
                                             DIGCF_PRESENT);
        if (devs == INVALID_HANDLE_VALUE) continue;

        SP_DEVINFO_DATA did;
        memset(&did, 0, sizeof(did));
        did.cbSize = sizeof(did);

        DWORD index = 0;
        while (SetupDiEnumDeviceInfo(devs, index, &did)) {
            index++;

            whwd_device *dev = (whwd_device *)calloc(1, sizeof(whwd_device));
            if (!dev) break;

            read_class(devs, &did, dev->device_class, sizeof(dev->device_class));
            read_device_id(devs, &did, dev->hwid, sizeof(dev->hwid));
            read_prop(devs, &did, SPDRP_FRIENDLYNAME, dev->name, sizeof(dev->name));
            if (!dev->name[0])
                read_prop(devs, &did, SPDRP_DEVICEDESC, dev->name, sizeof(dev->name));
            read_prop(devs, &did, SPDRP_MFG, dev->manufacturer, sizeof(dev->manufacturer));
            read_prop(devs, &did, SPDRP_SERVICE, dev->service, sizeof(dev->service));

            WCHAR instance[WHWD_INSTANCE_MAX] = {0};
            if (SetupDiGetDeviceInstanceIdW(devs, &did, instance,
                                            WHWD_INSTANCE_MAX, NULL))
                wstr_copy_utf8(instance, dev->instance_id, sizeof(dev->instance_id));

            WCHAR driver_path[512] = {0};
            if (SetupDiGetDeviceRegistryPropertyW(devs, &did, SPDRP_DRIVER, NULL,
                                                  (BYTE *)driver_path, sizeof(driver_path), NULL)) {
                char dp[512] = {0};
                wstr_copy_utf8(driver_path, dp, sizeof(dp));
                read_driver_registry(dp, dev);
            }

            whwd_device *grown = (whwd_device *)realloc(arr, (n + 1) * sizeof(whwd_device));
            if (!grown) {
                free(dev);
                break;
            }
            arr = grown;
            arr[n++] = *dev;
            free(dev);
        }

        SetupDiDestroyDeviceInfoList(devs);
    }

    *out = arr;
    *count = n;
    return 0;
}

void whwd_free_devices(whwd_device *devices)
{
    free(devices);
}