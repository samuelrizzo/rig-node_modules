#ifndef RIP_NODE_MODULES_PLATFORM_H
#define RIP_NODE_MODULES_PLATFORM_H

#include "common.h"

typedef struct {
    int exists;
    int is_directory;
    int is_symlink;
    uint64_t size;
    RnmIdentity identity;
} RnmFileInfo;

typedef struct {
    void *handle;
    void *state;
} RnmDirIter;

typedef enum {
    RNM_DIRENT_UNKNOWN = 0,
    RNM_DIRENT_FILE = 1,
    RNM_DIRENT_DIRECTORY = 2,
    RNM_DIRENT_SYMLINK = 3
} RnmDirEntryType;

typedef struct {
    const char *name;
    RnmDirEntryType type;
    int size_known;
    uint64_t size;
} RnmDirEntry;

int rnm_platform_list_volumes(RnmVolumeList *volumes);
void rnm_platform_free_volumes(RnmVolumeList *volumes);

int rnm_platform_lstat(const char *path, RnmFileInfo *info);
int rnm_platform_stat_follow(const char *path, RnmFileInfo *info);

int rnm_dir_iter_open(const char *path, RnmDirIter *iter);
int rnm_dir_iter_next(RnmDirIter *iter, RnmDirEntry *entry);
void rnm_dir_iter_close(RnmDirIter *iter);

int rnm_volume_total_bytes(const char *path, uint64_t *bytes);
uint32_t rnm_platform_get_last_error_code(void);
RnmErrorCategory rnm_platform_classify_error(uint32_t code);
const char *rnm_platform_error_category_name(RnmErrorCategory category);

#endif
