#ifndef WHWD_H
#define WHWD_H

#include <stddef.h>

#define WHWD_HWID_MAX       256
#define WHWD_NAME_MAX       256
#define WHWD_SEC_MAX        64
#define WHWD_VER_MAX        64
#define WHWD_DATE_MAX       64
#define WHWD_PROV_MAX       128
#define WHWD_INSTANCE_MAX   256
#define WHWD_UPDATE_ID_MAX  64
#define WHWD_TITLE_MAX      512
#define WHWD_URL_MAX        512
#define WHWD_SRC_MAX        32

typedef enum whwd_source_status {
    WHWD_SOURCE_OK = 0,
    WHWD_SOURCE_DISABLED,
    WHWD_SOURCE_UNAVAILABLE,
    WHWD_SOURCE_UNSUPPORTED_OS
} whwd_source_status;

typedef struct whwd_device {
    char hwid[WHWD_HWID_MAX];
    char name[WHWD_NAME_MAX];
    char manufacturer[WHWD_NAME_MAX];
    char service[WHWD_SEC_MAX];
    char driver_version[WHWD_VER_MAX];
    char driver_date[WHWD_DATE_MAX];
    char driver_provider[WHWD_PROV_MAX];
    char instance_id[WHWD_INSTANCE_MAX];
} whwd_device;

typedef struct whwd_features {
    int os_major;
    int os_minor;
    int os_build;
    int wua;
    int pnputil;
} whwd_features;

typedef struct whwd_update {
    char id[WHWD_UPDATE_ID_MAX];
    char source[WHWD_SRC_MAX];
    char title[WHWD_TITLE_MAX];
    char version[WHWD_VER_MAX];
    char date[WHWD_DATE_MAX];
    char provider[WHWD_PROV_MAX];
    char hwid[WHWD_HWID_MAX];
    char download_url[WHWD_URL_MAX];
} whwd_update;

int whwd_list_devices(whwd_device **out, size_t *count);
void whwd_free_devices(whwd_device *devices);

void whwd_detect_features(whwd_features *out);

int whwd_vercmp(const char *a, const char *b);

int whwd_check_updates(const whwd_features *features, whwd_update **out, size_t *count);
void whwd_free_updates(whwd_update *updates);

const char *whwd_source_name(size_t index);
size_t whwd_source_count(void);

#endif