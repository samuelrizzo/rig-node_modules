#include "platform.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>
#else
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/param.h>
#include <sys/mount.h>
#endif
#endif

static uint32_t rnm_last_platform_error_code = 0;

static void rnm_set_last_platform_error_code(uint32_t code) {
    rnm_last_platform_error_code = code;
}

uint32_t rnm_platform_get_last_error_code(void) {
    return rnm_last_platform_error_code;
}

const char *rnm_platform_error_category_name(RnmErrorCategory category) {
    switch (category) {
        case RNM_ERROR_PERMISSION_DENIED:
            return "permission denied";
        case RNM_ERROR_PATH_NOT_FOUND:
            return "path not found / disappeared";
        case RNM_ERROR_BUSY_OR_LOCKED:
            return "busy / locked";
        case RNM_ERROR_REPARSE_POINT:
            return "reparse point / symlink";
        case RNM_ERROR_INVALID_NAME:
            return "invalid name / path";
        case RNM_ERROR_DEVICE_IO:
            return "device / I/O";
        case RNM_ERROR_OTHER:
        default:
            return "other";
    }
}

#ifdef _WIN32
RnmErrorCategory rnm_platform_classify_error(uint32_t code) {
    switch (code) {
        case ERROR_ACCESS_DENIED:
        case ERROR_PRIVILEGE_NOT_HELD:
            return RNM_ERROR_PERMISSION_DENIED;
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_DIRECTORY:
        case ERROR_BAD_PATHNAME:
        case ERROR_NOT_READY:
            return RNM_ERROR_PATH_NOT_FOUND;
        case ERROR_SHARING_VIOLATION:
        case ERROR_LOCK_VIOLATION:
            return RNM_ERROR_BUSY_OR_LOCKED;
        case ERROR_CANT_ACCESS_FILE:
        case ERROR_CANT_RESOLVE_FILENAME:
        case ERROR_REPARSE_TAG_INVALID:
        case ERROR_REPARSE_TAG_MISMATCH:
        case ERROR_INVALID_REPARSE_DATA:
            return RNM_ERROR_REPARSE_POINT;
        case ERROR_INVALID_NAME:
        case ERROR_BUFFER_OVERFLOW:
        case ERROR_FILENAME_EXCED_RANGE:
            return RNM_ERROR_INVALID_NAME;
        case ERROR_IO_DEVICE:
        case ERROR_CRC:
        case ERROR_FILE_CORRUPT:
        case ERROR_DISK_CORRUPT:
        case ERROR_DEV_NOT_EXIST:
            return RNM_ERROR_DEVICE_IO;
        case 0:
            return RNM_ERROR_OTHER;
        default:
            return RNM_ERROR_OTHER;
    }
}
#else
RnmErrorCategory rnm_platform_classify_error(uint32_t code) {
    switch ((int) code) {
        case EACCES:
        case EPERM:
            return RNM_ERROR_PERMISSION_DENIED;
        case ENOENT:
        case ENOTDIR:
        case ESTALE:
            return RNM_ERROR_PATH_NOT_FOUND;
        case EBUSY:
        case ETXTBSY:
            return RNM_ERROR_BUSY_OR_LOCKED;
        case ELOOP:
            return RNM_ERROR_REPARSE_POINT;
        case ENAMETOOLONG:
        case EINVAL:
            return RNM_ERROR_INVALID_NAME;
        case EIO:
        case ENODEV:
            return RNM_ERROR_DEVICE_IO;
        case 0:
            return RNM_ERROR_OTHER;
        default:
            return RNM_ERROR_OTHER;
    }
}
#endif

void *rnm_malloc(size_t size) {
    void *memory = malloc(size);
    if (memory == NULL) {
        fprintf(stderr, "Out of memory.\n");
        exit(1);
    }
    return memory;
}

void *rnm_calloc(size_t count, size_t size) {
    void *memory = calloc(count, size);
    if (memory == NULL) {
        fprintf(stderr, "Out of memory.\n");
        exit(1);
    }
    return memory;
}

void *rnm_realloc(void *ptr, size_t size) {
    void *memory = realloc(ptr, size);
    if (memory == NULL) {
        fprintf(stderr, "Out of memory.\n");
        exit(1);
    }
    return memory;
}

char *rnm_strdup(const char *text) {
    size_t len = strlen(text) + 1U;
    char *copy = (char *) rnm_malloc(len);
    memcpy(copy, text, len);
    return copy;
}

static int path_is_sep(char ch) {
    return ch == '/' || ch == '\\';
}

char *rnm_path_join(const char *left, const char *right) {
    size_t left_len = strlen(left);
    size_t right_len = strlen(right);
    int needs_sep = left_len > 0 && !path_is_sep(left[left_len - 1]);
    char *result = (char *) rnm_malloc(left_len + right_len + (size_t) needs_sep + 1U);

    memcpy(result, left, left_len);
    if (needs_sep) {
#ifdef _WIN32
        result[left_len++] = '\\';
#else
        result[left_len++] = '/';
#endif
    }
    memcpy(result + left_len, right, right_len + 1U);
    return result;
}

const char *rnm_path_basename(const char *path) {
    size_t len = strlen(path);
    const char *end = path + len;

    while (end > path && path_is_sep(end[-1])) {
        --end;
    }
    while (end > path && !path_is_sep(end[-1])) {
        --end;
    }

    return end;
}

int rnm_path_is_dot_or_dotdot(const char *name) {
    return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
}

char *rnm_format_bytes(uint64_t bytes, char *buffer, size_t buffer_size) {
    static const char *suffixes[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    double value = (double) bytes;
    size_t suffix = 0;

    while (value >= 1024.0 && suffix + 1 < RNM_ARRAY_LEN(suffixes)) {
        value /= 1024.0;
        ++suffix;
    }

    snprintf(buffer, buffer_size, "%.2f %s", value, suffixes[suffix]);
    return buffer;
}

int rnm_casecmp(const char *left, const char *right) {
    while (*left != '\0' && *right != '\0') {
        int lhs = tolower((unsigned char) *left);
        int rhs = tolower((unsigned char) *right);
        if (lhs != rhs) {
            return lhs - rhs;
        }
        ++left;
        ++right;
    }
    return tolower((unsigned char) *left) - tolower((unsigned char) *right);
}

void rnm_path_stack_init(RnmPathStack *stack) {
    memset(stack, 0, sizeof(*stack));
}

void rnm_path_stack_free(RnmPathStack *stack) {
    size_t index;
    for (index = 0; index < stack->len; ++index) {
        free(stack->items[index]);
    }
    free(stack->items);
    memset(stack, 0, sizeof(*stack));
}

void rnm_path_stack_push(RnmPathStack *stack, char *path) {
    if (stack->len == stack->cap) {
        size_t new_cap = stack->cap == 0 ? 64U : stack->cap * 2U;
        stack->items = (char **) rnm_realloc(stack->items, sizeof(char *) * new_cap);
        stack->cap = new_cap;
    }
    stack->items[stack->len++] = path;
}

char *rnm_path_stack_pop(RnmPathStack *stack) {
    if (stack->len == 0) {
        return NULL;
    }
    return stack->items[--stack->len];
}

void rnm_volume_list_init(RnmVolumeList *list) {
    memset(list, 0, sizeof(*list));
}

void rnm_volume_list_push(RnmVolumeList *list, RnmVolume volume) {
    if (list->len == list->cap) {
        size_t new_cap = list->cap == 0 ? 8U : list->cap * 2U;
        list->items = (RnmVolume *) rnm_realloc(list->items, sizeof(RnmVolume) * new_cap);
        list->cap = new_cap;
    }
    list->items[list->len++] = volume;
}

void rnm_volume_list_free(RnmVolumeList *list) {
    size_t index;
    for (index = 0; index < list->len; ++index) {
        free(list->items[index].path);
        free(list->items[index].label);
    }
    free(list->items);
    memset(list, 0, sizeof(*list));
}

void rnm_occurrence_list_init(RnmOccurrenceList *list) {
    memset(list, 0, sizeof(*list));
}

void rnm_occurrence_list_push(RnmOccurrenceList *list, RnmOccurrence occurrence) {
    if (list->len == list->cap) {
        size_t new_cap = list->cap == 0 ? 64U : list->cap * 2U;
        list->items = (RnmOccurrence *) rnm_realloc(list->items, sizeof(RnmOccurrence) * new_cap);
        list->cap = new_cap;
    }
    list->items[list->len++] = occurrence;
}

void rnm_occurrence_list_free(RnmOccurrenceList *list) {
    size_t index;
    for (index = 0; index < list->len; ++index) {
        free(list->items[index].volume_label);
        free(list->items[index].path);
    }
    free(list->items);
    memset(list, 0, sizeof(*list));
}

static size_t identity_hash(RnmIdentity value) {
    uint64_t mixed = value.dev ^ (value.ino + 0x9e3779b97f4a7c15ULL + (value.dev << 6U) + (value.dev >> 2U));
    return (size_t) mixed;
}

void rnm_identity_set_init(RnmIdentitySet *set) {
    memset(set, 0, sizeof(*set));
}

void rnm_identity_set_free(RnmIdentitySet *set) {
    free(set->items);
    free(set->used);
    memset(set, 0, sizeof(*set));
}

static void identity_set_rehash(RnmIdentitySet *set, size_t new_cap) {
    RnmIdentity *old_items = set->items;
    unsigned char *old_used = set->used;
    size_t old_cap = set->cap;
    size_t index;

    set->items = (RnmIdentity *) rnm_calloc(new_cap, sizeof(RnmIdentity));
    set->used = (unsigned char *) rnm_calloc(new_cap, sizeof(unsigned char));
    set->cap = new_cap;
    set->len = 0;

    for (index = 0; index < old_cap; ++index) {
        if (old_used[index]) {
            rnm_identity_set_insert(set, old_items[index]);
        }
    }

    free(old_items);
    free(old_used);
}

int rnm_identity_set_insert(RnmIdentitySet *set, RnmIdentity value) {
    size_t index;

    if (set->cap == 0 || (set->len + 1U) * 10U >= set->cap * 7U) {
        size_t new_cap = set->cap == 0 ? 1024U : set->cap * 2U;
        identity_set_rehash(set, new_cap);
    }

    index = identity_hash(value) & (set->cap - 1U);
    while (set->used[index]) {
        if (set->items[index].dev == value.dev && set->items[index].ino == value.ino) {
            return 0;
        }
        index = (index + 1U) & (set->cap - 1U);
    }

    set->used[index] = 1;
    set->items[index] = value;
    set->len++;
    return 1;
}

#ifdef _WIN32
typedef struct {
    WIN32_FIND_DATAW data;
    int first_pending;
    char *name_buffer;
    size_t name_capacity;
} RnmWinDirState;

static wchar_t *utf8_to_wide(const char *text) {
    int count = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    wchar_t *wide;

    if (count <= 0) {
        rnm_set_last_platform_error_code(ERROR_NO_UNICODE_TRANSLATION);
        return NULL;
    }

    wide = (wchar_t *) rnm_malloc((size_t) count * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, count);
    return wide;
}

static int wide_to_utf8_buffer(const wchar_t *text, char **buffer, size_t *capacity) {
    int count = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);

    if (count <= 0) {
        rnm_set_last_platform_error_code(ERROR_NO_UNICODE_TRANSLATION);
        return 0;
    }

    if (*capacity < (size_t) count) {
        *buffer = (char *) rnm_realloc(*buffer, (size_t) count);
        *capacity = (size_t) count;
    }

    WideCharToMultiByte(CP_UTF8, 0, text, -1, *buffer, count, NULL, NULL);
    return 1;
}

static int stat_internal(const char *path, DWORD create_flags, RnmFileInfo *info) {
    wchar_t *wide = utf8_to_wide(path);
    HANDLE handle;
    BY_HANDLE_FILE_INFORMATION file_info;

    memset(info, 0, sizeof(*info));
    if (wide == NULL) {
        return 0;
    }

    handle = CreateFileW(
        wide,
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | create_flags,
        NULL
    );
    free(wide);

    if (handle == INVALID_HANDLE_VALUE) {
        rnm_set_last_platform_error_code((uint32_t) GetLastError());
        return 0;
    }

    if (!GetFileInformationByHandle(handle, &file_info)) {
        rnm_set_last_platform_error_code((uint32_t) GetLastError());
        CloseHandle(handle);
        return 0;
    }

    info->exists = 1;
    info->is_directory = (file_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    info->is_symlink = (file_info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    info->size = ((uint64_t) file_info.nFileSizeHigh << 32U) | (uint64_t) file_info.nFileSizeLow;
    info->identity.dev = (uint64_t) file_info.dwVolumeSerialNumber;
    info->identity.ino = ((uint64_t) file_info.nFileIndexHigh << 32U) | (uint64_t) file_info.nFileIndexLow;

    CloseHandle(handle);
    rnm_set_last_platform_error_code(0);
    return 1;
}

int rnm_platform_lstat(const char *path, RnmFileInfo *info) {
    return stat_internal(path, FILE_FLAG_OPEN_REPARSE_POINT, info);
}

int rnm_platform_stat_follow(const char *path, RnmFileInfo *info) {
    return stat_internal(path, 0, info);
}

int rnm_platform_list_volumes(RnmVolumeList *volumes) {
    DWORD mask = GetLogicalDrives();
    int index;

    for (index = 0; index < 26; ++index) {
        char root[] = "A:\\";
        UINT drive_type;
        uint64_t total_bytes = 0;

        if ((mask & (1U << index)) == 0U) {
            continue;
        }

        root[0] = (char) ('A' + index);
        drive_type = GetDriveTypeA(root);
        if (drive_type == DRIVE_NO_ROOT_DIR || drive_type == DRIVE_UNKNOWN) {
            continue;
        }

        rnm_volume_total_bytes(root, &total_bytes);
        rnm_volume_list_push(volumes, (RnmVolume) {
            rnm_strdup(root),
            rnm_strdup(root),
            total_bytes
        });
    }

    rnm_set_last_platform_error_code(0);
    return 1;
}

void rnm_platform_free_volumes(RnmVolumeList *volumes) {
    rnm_volume_list_free(volumes);
}

int rnm_dir_iter_open(const char *path, RnmDirIter *iter) {
    wchar_t *wide = utf8_to_wide(path);
    size_t len;
    wchar_t *pattern;
    HANDLE handle;
    RnmWinDirState *state;

    if (wide == NULL) {
        return 0;
    }

    len = wcslen(wide);
    pattern = (wchar_t *) rnm_malloc(sizeof(wchar_t) * (len + 3U));
    memcpy(pattern, wide, sizeof(wchar_t) * len);
    if (len > 0 && wide[len - 1] != L'\\' && wide[len - 1] != L'/') {
        pattern[len++] = L'\\';
    }
    pattern[len++] = L'*';
    pattern[len] = L'\0';

    state = (RnmWinDirState *) rnm_calloc(1, sizeof(RnmWinDirState));
    handle = FindFirstFileW(pattern, &state->data);
    free(pattern);
    free(wide);

    if (handle == INVALID_HANDLE_VALUE) {
        rnm_set_last_platform_error_code((uint32_t) GetLastError());
        free(state);
        return 0;
    }

    state->first_pending = 1;
    iter->handle = handle;
    iter->state = state;
    rnm_set_last_platform_error_code(0);
    return 1;
}

int rnm_dir_iter_next(RnmDirIter *iter, RnmDirEntry *entry) {
    HANDLE handle = (HANDLE) iter->handle;
    RnmWinDirState *state = (RnmWinDirState *) iter->state;

    if (state == NULL || handle == INVALID_HANDLE_VALUE) {
        rnm_set_last_platform_error_code(ERROR_INVALID_HANDLE);
        return 0;
    }

    if (state->first_pending) {
        state->first_pending = 0;
    } else if (!FindNextFileW(handle, &state->data)) {
        rnm_set_last_platform_error_code((uint32_t) GetLastError());
        return 0;
    }

    if (!wide_to_utf8_buffer(state->data.cFileName, &state->name_buffer, &state->name_capacity)) {
        return 0;
    }

    entry->name = state->name_buffer;
    entry->size = ((uint64_t) state->data.nFileSizeHigh << 32U) | (uint64_t) state->data.nFileSizeLow;
    entry->size_known = (state->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;

    if ((state->data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        entry->type = RNM_DIRENT_SYMLINK;
    } else if ((state->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        entry->type = RNM_DIRENT_DIRECTORY;
    } else {
        entry->type = RNM_DIRENT_FILE;
    }

    rnm_set_last_platform_error_code(0);
    return 1;
}

void rnm_dir_iter_close(RnmDirIter *iter) {
    RnmWinDirState *state = (RnmWinDirState *) iter->state;

    if (iter->handle != NULL && iter->handle != INVALID_HANDLE_VALUE) {
        FindClose((HANDLE) iter->handle);
    }
    if (state != NULL) {
        free(state->name_buffer);
    }

    free(state);
    iter->handle = NULL;
    iter->state = NULL;
}

int rnm_volume_total_bytes(const char *path, uint64_t *bytes) {
    wchar_t *wide = utf8_to_wide(path);
    ULARGE_INTEGER total;

    if (wide == NULL) {
        return 0;
    }
    if (!GetDiskFreeSpaceExW(wide, NULL, &total, NULL)) {
        rnm_set_last_platform_error_code((uint32_t) GetLastError());
        free(wide);
        return 0;
    }

    free(wide);
    *bytes = total.QuadPart;
    rnm_set_last_platform_error_code(0);
    return 1;
}
#else
int rnm_platform_lstat(const char *path, RnmFileInfo *info) {
    struct stat st;

    memset(info, 0, sizeof(*info));
    if (lstat(path, &st) != 0) {
        rnm_set_last_platform_error_code((uint32_t) errno);
        return 0;
    }

    info->exists = 1;
    info->is_directory = S_ISDIR(st.st_mode);
    info->is_symlink = S_ISLNK(st.st_mode);
    info->size = (uint64_t) st.st_size;
    info->identity.dev = (uint64_t) st.st_dev;
    info->identity.ino = (uint64_t) st.st_ino;
    rnm_set_last_platform_error_code(0);
    return 1;
}

int rnm_platform_stat_follow(const char *path, RnmFileInfo *info) {
    struct stat st;

    memset(info, 0, sizeof(*info));
    if (stat(path, &st) != 0) {
        rnm_set_last_platform_error_code((uint32_t) errno);
        return 0;
    }

    info->exists = 1;
    info->is_directory = S_ISDIR(st.st_mode);
    info->is_symlink = S_ISLNK(st.st_mode);
    info->size = (uint64_t) st.st_size;
    info->identity.dev = (uint64_t) st.st_dev;
    info->identity.ino = (uint64_t) st.st_ino;
    rnm_set_last_platform_error_code(0);
    return 1;
}

int rnm_platform_list_volumes(RnmVolumeList *volumes) {
#if defined(__linux__)
    FILE *mounts = fopen("/proc/self/mounts", "r");
    char device[512];
    char mountpoint[512];
    char fstype[128];

    if (mounts == NULL) {
        rnm_set_last_platform_error_code((uint32_t) errno);
        return 0;
    }

    while (fscanf(mounts, "%511s %511s %127s%*[^\n]\n", device, mountpoint, fstype) == 3) {
        uint64_t total_bytes = 0;

        (void) device;
        (void) fstype;

        if (strncmp(mountpoint, "/proc", 5) == 0 || strncmp(mountpoint, "/sys", 4) == 0 ||
            strncmp(mountpoint, "/dev", 4) == 0 || strncmp(mountpoint, "/run", 4) == 0) {
            continue;
        }

        if (!rnm_volume_total_bytes(mountpoint, &total_bytes)) {
            continue;
        }

        rnm_volume_list_push(volumes, (RnmVolume) {
            rnm_strdup(mountpoint),
            rnm_strdup(mountpoint),
            total_bytes
        });
    }

    fclose(mounts);
    rnm_set_last_platform_error_code(0);
    return 1;
#else
    struct statfs *mounts = NULL;
    int count;
    int index;

    count = getmntinfo(&mounts, MNT_NOWAIT);
    if (count <= 0) {
        rnm_set_last_platform_error_code((uint32_t) errno);
        return 0;
    }

    for (index = 0; index < count; ++index) {
        uint64_t total_bytes = 0;
        if (!rnm_volume_total_bytes(mounts[index].f_mntonname, &total_bytes)) {
            continue;
        }

        rnm_volume_list_push(volumes, (RnmVolume) {
            rnm_strdup(mounts[index].f_mntonname),
            rnm_strdup(mounts[index].f_mntonname),
            total_bytes
        });
    }

    rnm_set_last_platform_error_code(0);
    return 1;
#endif
}

void rnm_platform_free_volumes(RnmVolumeList *volumes) {
    rnm_volume_list_free(volumes);
}

int rnm_dir_iter_open(const char *path, RnmDirIter *iter) {
    DIR *dir = opendir(path);

    if (dir == NULL) {
        rnm_set_last_platform_error_code((uint32_t) errno);
        return 0;
    }

    iter->handle = dir;
    iter->state = NULL;
    rnm_set_last_platform_error_code(0);
    return 1;
}

int rnm_dir_iter_next(RnmDirIter *iter, RnmDirEntry *entry) {
    struct dirent *dir_entry;

    errno = 0;
    dir_entry = readdir((DIR *) iter->handle);
    if (dir_entry == NULL) {
        rnm_set_last_platform_error_code((uint32_t) errno);
        return 0;
    }

    entry->name = dir_entry->d_name;
    entry->size = 0;
    entry->size_known = 0;

#if defined(_DIRENT_HAVE_D_TYPE)
    switch (dir_entry->d_type) {
        case DT_DIR:
            entry->type = RNM_DIRENT_DIRECTORY;
            break;
        case DT_REG:
            entry->type = RNM_DIRENT_FILE;
            break;
        case DT_LNK:
            entry->type = RNM_DIRENT_SYMLINK;
            break;
        default:
            entry->type = RNM_DIRENT_UNKNOWN;
            break;
    }
#else
    entry->type = RNM_DIRENT_UNKNOWN;
#endif

    rnm_set_last_platform_error_code(0);
    return 1;
}

void rnm_dir_iter_close(RnmDirIter *iter) {
    if (iter->handle != NULL) {
        closedir((DIR *) iter->handle);
    }
    iter->handle = NULL;
    iter->state = NULL;
}

int rnm_volume_total_bytes(const char *path, uint64_t *bytes) {
    struct statvfs fs;

    if (statvfs(path, &fs) != 0) {
        rnm_set_last_platform_error_code((uint32_t) errno);
        return 0;
    }

    *bytes = (uint64_t) fs.f_blocks * (uint64_t) fs.f_frsize;
    rnm_set_last_platform_error_code(0);
    return 1;
}
#endif
