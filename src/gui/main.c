#include "whwd/whwd.h"
#include "whwd_version.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <windows.h>
#include <commctrl.h>

#define IDC_BTN_REFRESH 1001
#define IDC_BTN_CHECK   1002
#define IDC_BTN_INSTALL 1003

#define WM_APP_DEVICES (WM_APP + 0)
#define WM_APP_UPDATES (WM_APP + 1)

static HWND g_hwnd_list;
static HWND g_hwnd_status;
static HWND g_btn_refresh;
static HWND g_btn_check;
static HWND g_btn_install;

static whwd_features g_features;
static int g_scanning_devices;
static int g_scanning_updates;
static int g_view_mode;
static int g_columns;

static WCHAR g_group_names[128][WHWD_CLASS_MAX];
static int g_group_count;

enum {
    VIEW_NONE = 0,
    VIEW_DEVICES,
    VIEW_UPDATES,
};

static void utf8_to_wide(const char *in, WCHAR *out, size_t outsz)
{
    if (!in || !out || outsz == 0) return;
    out[0] = 0;
    MultiByteToWideChar(CP_UTF8, 0, in, -1, out, (int)outsz);
}

static void clear_list(HWND list)
{
    ListView_DeleteAllItems(list);
    while (ListView_DeleteColumn(list, 0)) {
    }
    g_columns = 0;
}

static void add_column(HWND list, const WCHAR *title, int width)
{
    LVCOLUMNW col;
    memset(&col, 0, sizeof(col));
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    col.cx = width;
    col.pszText = (WCHAR *)title;
    col.iSubItem = g_columns;
    ListView_InsertColumn(list, g_columns, &col);
    g_columns++;
}

static void set_cell(HWND list, int row, int col, const char *text)
{
    WCHAR wide[1024];
    utf8_to_wide(text, wide, sizeof(wide) / sizeof(wide[0]));

    LVITEMW item;
    memset(&item, 0, sizeof(item));
    item.mask = LVIF_TEXT;
    item.iItem = row;
    item.iSubItem = col;
    item.pszText = wide;

    if (col == 0)
        ListView_InsertItem(list, &item);
    else
        ListView_SetItem(list, &item);
}

static void set_grouped_cell(HWND list, int row, int col, const char *text, int group_id)
{
    WCHAR wide[1024];
    utf8_to_wide(text, wide, sizeof(wide) / sizeof(wide[0]));

    LVITEMW item;
    memset(&item, 0, sizeof(item));
    item.mask = LVIF_TEXT | (col == 0 ? LVIF_GROUPID : 0);
    item.iItem = row;
    item.iSubItem = col;
    item.pszText = wide;
    item.iGroupId = group_id;

    if (col == 0)
        ListView_InsertItem(list, &item);
    else
        ListView_SetItem(list, &item);
}

static int group_ensure(HWND list, const char *class_name)
{
    WCHAR wide[WHWD_CLASS_MAX];
    utf8_to_wide(class_name && class_name[0] ? class_name : "Other devices",
                 wide, WHWD_CLASS_MAX);

    for (int i = 0; i < g_group_count; i++) {
        if (0 == _wcsicmp(g_group_names[i], wide))
            return i + 1;
    }

    if (g_group_count >= (int)(sizeof(g_group_names) / sizeof(g_group_names[0])))
        return 0;

    LVGROUP lg;
    memset(&lg, 0, sizeof(lg));
    lg.cbSize = sizeof(lg);
    lg.mask = LVGF_HEADER | LVGF_GROUPID;
    lg.pszHeader = wide;
    lg.iGroupId = g_group_count + 1;
    ListView_InsertGroup(list, -1, &lg);
    wcsncpy(g_group_names[g_group_count], wide, WHWD_CLASS_MAX - 1);
    g_group_names[g_group_count][WHWD_CLASS_MAX - 1] = 0;
    g_group_count++;
    return g_group_count;
}

static void groups_reset(HWND list)
{
    g_group_count = 0;
    ListView_RemoveAllGroups(list);
}

static void set_status_text(const WCHAR *text)
{
    SendMessageW(g_hwnd_status, SB_SETTEXT, 0, (LPARAM)text);
}

static void set_status_fmt(const WCHAR *fmt, ...)
{
    WCHAR buf[512];
    va_list ap;
    va_start(ap, fmt);
    vswprintf(buf, 512, fmt, ap);
    va_end(ap);
    set_status_text(buf);
}

static void show_features(void)
{
    set_status_fmt(L"Windows %d.%d.%d  WUA: %s  pnputil: %s",
                   g_features.os_major, g_features.os_minor, g_features.os_build,
                   g_features.wua ? L"yes" : L"no",
                   g_features.pnputil ? L"yes" : L"no");
}

static void switch_view(int mode)
{
    g_view_mode = mode;
    groups_reset(g_hwnd_list);
    clear_list(g_hwnd_list);

    if (mode == VIEW_DEVICES) {
        add_column(g_hwnd_list, L"Name", 230);
        add_column(g_hwnd_list, L"Hardware ID", 210);
        add_column(g_hwnd_list, L"Driver version", 100);
        add_column(g_hwnd_list, L"Class", 160);
    } else if (mode == VIEW_UPDATES) {
        add_column(g_hwnd_list, L"ID", 70);
        add_column(g_hwnd_list, L"Source", 130);
        add_column(g_hwnd_list, L"Title", 500);
    }
}

static DWORD WINAPI devices_worker(LPVOID param)
{
    HWND hwnd = (HWND)param;
    whwd_device *devs = NULL;
    size_t n = 0;
    if (whwd_list_devices(&devs, &n) != 0) n = 0;
    PostMessage(hwnd, WM_APP_DEVICES, (WPARAM)n, (LPARAM)devs);
    return 0;
}

static DWORD WINAPI updates_worker(LPVOID param)
{
    HWND hwnd = (HWND)param;
    whwd_update *updates = NULL;
    size_t n = 0;
    if (whwd_check_updates(&g_features, &updates, &n) != 0) n = 0;
    PostMessage(hwnd, WM_APP_UPDATES, (WPARAM)n, (LPARAM)updates);
    return 0;
}

static void start_devices_scan(HWND hwnd)
{
    if (g_scanning_devices) return;
    g_scanning_devices = 1;
    EnableWindow(g_btn_refresh, FALSE);
    set_status_text(L"Enumerating devices...");
    CreateThread(NULL, 0, devices_worker, hwnd, 0, NULL);
}

static void start_updates_scan(HWND hwnd)
{
    if (g_scanning_updates) return;
    g_scanning_updates = 1;
    EnableWindow(g_btn_check, FALSE);
    set_status_text(L"Checking for updates...");
    CreateThread(NULL, 0, updates_worker, hwnd, 0, NULL);
}

static void on_install_pressed(void)
{
    if (g_view_mode != VIEW_UPDATES) {
        MessageBoxW(NULL, L"Check for updates first, then pick one to install.",
                    L"whwd", MB_OK | MB_ICONINFORMATION);
        return;
    }

    int sel = ListView_GetNextItem(g_hwnd_list, -1, LVNI_SELECTED);
    if (sel < 0) {
        MessageBoxW(NULL, L"Select an update from the list first.",
                    L"whwd", MB_OK | MB_ICONINFORMATION);
        return;
    }

    MessageBoxW(NULL, L"Installation is not implemented yet.",
                L"whwd", MB_OK | MB_ICONINFORMATION);
}

static void on_devices_done(size_t n, whwd_device *devs)
{
    switch_view(VIEW_DEVICES);
    for (size_t i = 0; i < n; i++) {
        const whwd_device *d = &devs[i];
        int gid = group_ensure(g_hwnd_list, d->device_class);
        set_grouped_cell(g_hwnd_list, (int)i, 0, d->name[0] ? d->name : d->hwid, gid);
        set_cell(g_hwnd_list, (int)i, 1, d->hwid);
        set_cell(g_hwnd_list, (int)i, 2, d->driver_version[0] ? d->driver_version : "-");
        set_cell(g_hwnd_list, (int)i, 3, d->device_class[0] ? d->device_class : "-");
    }
    set_status_fmt(L"%u devices detected in %d classes",
                   (unsigned)n, g_group_count);
    free(devs);
    g_scanning_devices = 0;
    EnableWindow(g_btn_refresh, TRUE);
}

static void on_updates_done(size_t n, whwd_update *updates)
{
    switch_view(VIEW_UPDATES);
    for (size_t i = 0; i < n; i++) {
        const whwd_update *u = &updates[i];
        set_cell(g_hwnd_list, (int)i, 0, u->id);
        set_cell(g_hwnd_list, (int)i, 1, u->source);
        set_cell(g_hwnd_list, (int)i, 2, u->title);
    }
    set_status_fmt(L"%u updates available", (unsigned)n);
    free(updates);
    g_scanning_updates = 0;
    EnableWindow(g_btn_check, TRUE);
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
    case WM_COMMAND: {
        int id = LOWORD(wparam);
        if (id == IDC_BTN_REFRESH) start_devices_scan(hwnd);
        else if (id == IDC_BTN_CHECK) start_updates_scan(hwnd);
        else if (id == IDC_BTN_INSTALL) on_install_pressed();
        break;
    }

    case WM_APP_DEVICES:
        on_devices_done((size_t)wparam, (whwd_device *)lparam);
        break;

    case WM_APP_UPDATES:
        on_updates_done((size_t)wparam, (whwd_update *)lparam);
        break;

    case WM_SIZE: {
        SendMessageW(g_hwnd_status, WM_SIZE, 0, 0);
        RECT client;
        GetClientRect(hwnd, &client);
        RECT status;
        GetWindowRect(g_hwnd_status, &status);
        int status_h = status.bottom - status.top;
        MoveWindow(g_hwnd_list, 8, 40,
                   client.right - 16, client.bottom - 40 - status_h - 4, TRUE);
        break;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE h_instance, HINSTANCE h_prev,
                    PWSTR cmd_line, int n_show)
{
    (void)h_prev;
    (void)cmd_line;
    (void)n_show;

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = h_instance;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"whwd_gui_class";
    if (!RegisterClassExW(&wc)) return 1;

    HWND hwnd = CreateWindowExW(
        0, L"whwd_gui_class", L"whwd - Windows Hardware Detection",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 780, 520,
        NULL, NULL, h_instance, NULL);
    if (!hwnd) return 1;

    g_btn_refresh = CreateWindowW(L"BUTTON", L"Refresh devices",
                                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                  8, 8, 120, 26, hwnd, (HMENU)IDC_BTN_REFRESH,
                                  h_instance, NULL);
    g_btn_check = CreateWindowW(L"BUTTON", L"Check updates",
                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                136, 8, 120, 26, hwnd, (HMENU)IDC_BTN_CHECK,
                                h_instance, NULL);
    g_btn_install = CreateWindowW(L"BUTTON", L"Install",
                                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                  264, 8, 100, 26, hwnd, (HMENU)IDC_BTN_INSTALL,
                                  h_instance, NULL);

    g_hwnd_list = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        8, 40, 700, 400, hwnd, NULL, h_instance, NULL);
    ListView_SetExtendedListViewStyle(g_hwnd_list,
                                      LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES |
                                          LVS_EX_GROUPVIEW);

    g_hwnd_status = CreateWindowW(STATUSCLASSNAMEW, NULL,
                                  WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                                  0, 0, 0, 0, hwnd, NULL, h_instance, NULL);

    whwd_detect_features(&g_features);
    show_features();

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}