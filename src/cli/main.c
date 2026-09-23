#include "whwd/whwd.h"
#include "whwd_version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

static void print_help(void)
{
    printf(
        "whwd - Windows Hardware Detection and driver updater\n"
        "\n"
        "Usage: whwd [options]\n"
        "\n"
        "  -d, --devices     list detected hardware and installed drivers\n"
        "  -l, --list        list available driver updates from all sources\n"
        "  -i, --install ID  install the given update\n"
        "  -j, --json        emit machine-readable JSON output\n"
        "  -V, --version     print version and exit\n"
        "  -h, --help        show this help and exit\n");
}

static void print_version(void)
{
    printf("whwd %s\n", WHWD_VERSION_STRING);
}

static void json_escape(const char *in, char *out, size_t outsz)
{
    if (!out || outsz == 0) return;
    size_t o = 0;
    if (in) {
        for (size_t i = 0; in[i] && o + 7 < outsz; i++) {
            unsigned char c = (unsigned char)in[i];
            switch (c) {
            case '"':
                out[o++] = '\\';
                out[o++] = '"';
                break;
            case '\\':
                out[o++] = '\\';
                out[o++] = '\\';
                break;
            case '\n':
                out[o++] = '\\';
                out[o++] = 'n';
                break;
            case '\r':
                out[o++] = '\\';
                out[o++] = 'r';
                break;
            case '\t':
                out[o++] = '\\';
                out[o++] = 't';
                break;
            default:
                if (c < 0x20) {
                    if (o + 6 < outsz)
                        o += (size_t)snprintf(out + o, 7, "\\u%04x", c);
                } else {
                    out[o++] = (char)c;
                }
            }
        }
    }
    out[o] = 0;
}

static void json_puts(const char *str)
{
    char esc[4096];
    json_escape(str, esc, sizeof(esc));
    printf("\"%s\"", esc);
}

static void print_features(const whwd_features *feat)
{
    printf("OS: %d.%d.%d  Windows Update API: %s  pnputil: %s\n",
           feat->os_major, feat->os_minor, feat->os_build,
           feat->wua ? "available" : "unavailable",
           feat->pnputil ? "available" : "unavailable");
}

static void print_devices_human(const whwd_features *feat,
                                const whwd_device *devs, size_t n)
{
    print_features(feat);
    printf("\n");
    if (n == 0) {
        printf("no devices found\n");
        return;
    }
    printf("%-4s  %-36s  %-30s  %s\n", "idx", "name", "hardware id", "driver version");
    for (size_t i = 0; i < n; i++) {
        const whwd_device *d = &devs[i];
        printf("%-4zu  %-36.36s  %-30.30s  %s\n",
               i,
               d->name[0] ? d->name : d->hwid,
               d->hwid,
               d->driver_version[0] ? d->driver_version : "(none)");
    }
}

static void print_devices_json(const whwd_features *feat,
                               const whwd_device *devs, size_t n)
{
    printf("{\"whwd\":\"%s\",\"os\":{\"major\":%d,\"minor\":%d,\"build\":%d},"
           "\"features\":{\"wua\":%s,\"pnputil\":%s},\"device_count\":%zu,\"devices\":[",
           WHWD_VERSION_STRING,
           feat->os_major, feat->os_minor, feat->os_build,
           feat->wua ? "true" : "false",
           feat->pnputil ? "true" : "false",
           n);
    for (size_t i = 0; i < n; i++) {
        const whwd_device *d = &devs[i];
        if (i) printf(",");
        printf("{\"index\":%zu,\"name\":", i);
        json_puts(d->name);
        printf(",\"hwid\":");
        json_puts(d->hwid);
        printf(",\"manufacturer\":");
        json_puts(d->manufacturer);
        printf(",\"service\":");
        json_puts(d->service);
        printf(",\"driver_version\":");
        json_puts(d->driver_version);
        printf(",\"driver_date\":");
        json_puts(d->driver_date);
        printf(",\"driver_provider\":");
        json_puts(d->driver_provider);
        printf(",\"instance_id\":");
        json_puts(d->instance_id);
        printf("}");
    }
    printf("]}\n");
}

static void print_updates_human(const whwd_features *feat,
                                const whwd_update *updates, size_t n)
{
    print_features(feat);
    printf("\n");
    if (n == 0) {
        printf("no updates available from any source\n");
        return;
    }
    printf("%-4s  %-16s  %s\n", "id", "source", "title");
    for (size_t i = 0; i < n; i++) {
        const whwd_update *u = &updates[i];
        printf("%-4s  %-16.16s  %s\n", u->id, u->source, u->title);
    }
}

static void print_updates_json(const whwd_features *feat,
                               const whwd_update *updates, size_t n)
{
    printf("{\"whwd\":\"%s\",\"os\":{\"major\":%d,\"minor\":%d,\"build\":%d},"
           "\"features\":{\"wua\":%s,\"pnputil\":%s},\"update_count\":%zu,\"updates\":[",
           WHWD_VERSION_STRING,
           feat->os_major, feat->os_minor, feat->os_build,
           feat->wua ? "true" : "false",
           feat->pnputil ? "true" : "false",
           n);
    for (size_t i = 0; i < n; i++) {
        const whwd_update *u = &updates[i];
        if (i) printf(",");
        printf("{\"id\":");
        json_puts(u->id);
        printf(",\"source\":");
        json_puts(u->source);
        printf(",\"title\":");
        json_puts(u->title);
        printf(",\"version\":");
        json_puts(u->version);
        printf(",\"date\":");
        json_puts(u->date);
        printf(",\"provider\":");
        json_puts(u->provider);
        printf(",\"hwid\":");
        json_puts(u->hwid);
        printf(",\"download_url\":");
        json_puts(u->download_url);
        printf("}");
    }
    printf("]}\n");
}

int main(int argc, char **argv)
{
    SetConsoleOutputCP(CP_UTF8);

    int want_devices = 0;
    int want_list = 0;
    int want_json = 0;
    const char *install_id = NULL;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (!strcmp(arg, "-d") || !strcmp(arg, "--devices")) {
            want_devices = 1;
        } else if (!strcmp(arg, "-l") || !strcmp(arg, "--list")) {
            want_list = 1;
        } else if (!strcmp(arg, "-i") || !strcmp(arg, "--install")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "whwd: --install requires an update id\n");
                return 2;
            }
            install_id = argv[++i];
        } else if (!strcmp(arg, "-j") || !strcmp(arg, "--json")) {
            want_json = 1;
        } else if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            print_help();
            return 0;
        } else if (!strcmp(arg, "-V") || !strcmp(arg, "--version")) {
            print_version();
            return 0;
        } else {
            fprintf(stderr, "whwd: unknown option '%s'\n\n", arg);
            print_help();
            return 2;
        }
    }

    if (!want_devices && !want_list && !install_id) {
        print_help();
        return 0;
    }

    whwd_features feat;
    whwd_detect_features(&feat);

    if (install_id) {
        fprintf(stderr, "whwd: installation is not implemented yet (update '%s')\n",
                install_id);
        return 1;
    }

    int rc = 0;

    if (want_devices) {
        whwd_device *devs = NULL;
        size_t ndevs = 0;
        if (whwd_list_devices(&devs, &ndevs) != 0) {
            fprintf(stderr, "whwd: failed to enumerate devices\n");
            rc = 1;
        } else if (want_json) {
            print_devices_json(&feat, devs, ndevs);
        } else {
            print_devices_human(&feat, devs, ndevs);
        }
        whwd_free_devices(devs);
    }

    if (want_list) {
        whwd_update *updates = NULL;
        size_t n = 0;
        if (whwd_check_updates(&feat, &updates, &n) != 0) {
            fprintf(stderr, "whwd: failed to check updates\n");
            rc = 1;
        } else if (want_json) {
            print_updates_json(&feat, updates, n);
        } else {
            print_updates_human(&feat, updates, n);
        }
        whwd_free_updates(updates);
    }

    return rc;
}