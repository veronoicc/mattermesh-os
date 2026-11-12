#pragma once
// NVS entry iteration as the firmware uses it: telling a clean flash (empty NVS) from an installed device, and the backup
// (every entry of every namespace). The values themselves are read through Preferences.
#include <cstddef>
bool emuNvsHasEntries();
typedef void *nvs_iterator_t;
typedef enum { NVS_TYPE_U8 = 0x01, NVS_TYPE_I8 = 0x11, NVS_TYPE_U16 = 0x02, NVS_TYPE_I16 = 0x12, NVS_TYPE_U32 = 0x04, NVS_TYPE_I32 = 0x14,
               NVS_TYPE_U64 = 0x08, NVS_TYPE_I64 = 0x18, NVS_TYPE_STR = 0x21, NVS_TYPE_BLOB = 0x42, NVS_TYPE_ANY = 0xff } nvs_type_t;
typedef struct { char namespace_name[16]; char key[16]; nvs_type_t type; } nvs_entry_info_t;
#define NVS_DEFAULT_PART_NAME "nvs"
nvs_iterator_t nvs_entry_find(const char *, const char *, nvs_type_t);
nvs_iterator_t nvs_entry_next(nvs_iterator_t);
void nvs_entry_info(nvs_iterator_t, nvs_entry_info_t *);
void nvs_release_iterator(nvs_iterator_t);
