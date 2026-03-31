#ifndef RIP_NODE_MODULES_COMMON_H
#define RIP_NODE_MODULES_COMMON_H

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#define RNM_ARRAY_LEN(value) (sizeof(value) / sizeof((value)[0]))

typedef struct {
    uint64_t dev;
    uint64_t ino;
} RnmIdentity;

typedef enum {
    RNM_ERROR_PERMISSION_DENIED = 0,
    RNM_ERROR_PATH_NOT_FOUND = 1,
    RNM_ERROR_BUSY_OR_LOCKED = 2,
    RNM_ERROR_REPARSE_POINT = 3,
    RNM_ERROR_INVALID_NAME = 4,
    RNM_ERROR_DEVICE_IO = 5,
    RNM_ERROR_OTHER = 6,
    RNM_ERROR_CATEGORY_COUNT = 7
} RnmErrorCategory;

typedef struct {
    char *path;
    char *label;
    uint64_t total_bytes;
} RnmVolume;

typedef struct {
    RnmVolume *items;
    size_t len;
    size_t cap;
} RnmVolumeList;

typedef struct {
    char *volume_label;
    char *path;
    uint64_t bytes;
} RnmOccurrence;

typedef struct {
    RnmOccurrence *items;
    size_t len;
    size_t cap;
} RnmOccurrenceList;

typedef struct {
    char **items;
    size_t len;
    size_t cap;
} RnmPathStack;

typedef struct {
    RnmIdentity *items;
    unsigned char *used;
    size_t cap;
    size_t len;
} RnmIdentitySet;

typedef struct {
    uint64_t visited_directories;
    uint64_t visited_files;
    uint64_t skipped_duplicates;
    uint64_t access_errors;
    uint64_t error_categories[RNM_ERROR_CATEGORY_COUNT];
    uint64_t node_modules_found;
    uint64_t total_node_modules_bytes;
} RnmStats;

typedef struct {
    uint64_t scan_ms;
    uint64_t report_ms;
    uint64_t total_ms;
} RnmRunTimings;

void *rnm_malloc(size_t size);
void *rnm_calloc(size_t count, size_t size);
void *rnm_realloc(void *ptr, size_t size);
char *rnm_strdup(const char *text);
char *rnm_path_join(const char *left, const char *right);
const char *rnm_path_basename(const char *path);
int rnm_path_is_dot_or_dotdot(const char *name);
char *rnm_format_bytes(uint64_t bytes, char *buffer, size_t buffer_size);
int rnm_casecmp(const char *left, const char *right);

void rnm_path_stack_init(RnmPathStack *stack);
void rnm_path_stack_free(RnmPathStack *stack);
void rnm_path_stack_push(RnmPathStack *stack, char *path);
char *rnm_path_stack_pop(RnmPathStack *stack);

void rnm_volume_list_init(RnmVolumeList *list);
void rnm_volume_list_push(RnmVolumeList *list, RnmVolume volume);
void rnm_volume_list_free(RnmVolumeList *list);

void rnm_occurrence_list_init(RnmOccurrenceList *list);
void rnm_occurrence_list_push(RnmOccurrenceList *list, RnmOccurrence occurrence);
void rnm_occurrence_list_free(RnmOccurrenceList *list);

void rnm_identity_set_init(RnmIdentitySet *set);
void rnm_identity_set_free(RnmIdentitySet *set);
int rnm_identity_set_insert(RnmIdentitySet *set, RnmIdentity value);

#endif
