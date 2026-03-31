#include "scanner.h"

#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define RNM_NO_PARENT ((size_t) -1)

typedef struct {
    uint64_t last_update_ms;
    uint64_t frame;
} RnmProgressState;

typedef enum {
    RNM_SCAN_ENTER = 0,
    RNM_SCAN_LEAVE = 1
} RnmScanStage;

typedef struct {
    char *path;
    size_t parent_index;
    uint64_t subtree_bytes;
    unsigned char stage;
    unsigned char inside_node_modules;
    unsigned char root_node_modules;
    unsigned char named_node_modules;
} RnmScanFrame;

typedef struct {
    RnmScanFrame *items;
    size_t len;
    size_t cap;
} RnmScanFrameList;

typedef struct {
    size_t *items;
    size_t len;
    size_t cap;
} RnmIndexStack;

static uint64_t now_ms(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return ((uint64_t) ts.tv_sec * 1000U) + ((uint64_t) ts.tv_nsec / 1000000U);
}

static void print_progress(
    const char *label,
    const RnmStats *stats,
    RnmProgressState *progress,
    const RnmScanOptions *options,
    int force
) {
    enum { BAR_WIDTH = 24 };
    char bar[BAR_WIDTH + 1];
    char bytes_buffer[32];
    uint64_t current_ms;
    size_t index;
    size_t marker;

    if (options == NULL || !options->show_progress) {
        return;
    }

    current_ms = now_ms();
    if (!force && current_ms - progress->last_update_ms < 125U) {
        return;
    }

    marker = (size_t) (progress->frame % (uint64_t) (BAR_WIDTH * 2U));
    if (marker >= BAR_WIDTH) {
        marker = (size_t) ((BAR_WIDTH * 2U - 1U) - marker);
    }

    for (index = 0; index < BAR_WIDTH; ++index) {
        size_t distance = index > marker ? index - marker : marker - index;
        bar[index] = distance == 0 ? '#' : (distance == 1 ? '=' : '-');
    }
    bar[BAR_WIDTH] = '\0';

    printf(
        "\r[%s] %-12s dirs=%" PRIu64 " files=%" PRIu64 " node_modules=%" PRIu64 " impact=%s",
        bar,
        label,
        stats->visited_directories,
        stats->visited_files,
        stats->node_modules_found,
        rnm_format_bytes(stats->total_node_modules_bytes, bytes_buffer, sizeof(bytes_buffer))
    );
    fflush(stdout);
    progress->last_update_ms = current_ms;
    progress->frame++;
}

static void frame_list_init(RnmScanFrameList *list) {
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
}

static void frame_list_free(RnmScanFrameList *list) {
    size_t index;
    for (index = 0; index < list->len; ++index) {
        free(list->items[index].path);
    }
    free(list->items);
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
}

static size_t frame_list_push(RnmScanFrameList *list, RnmScanFrame frame) {
    if (list->len == list->cap) {
        size_t new_cap = list->cap == 0 ? 256U : list->cap * 2U;
        list->items = (RnmScanFrame *) rnm_realloc(list->items, sizeof(RnmScanFrame) * new_cap);
        list->cap = new_cap;
    }
    list->items[list->len] = frame;
    return list->len++;
}

static void index_stack_init(RnmIndexStack *stack) {
    stack->items = NULL;
    stack->len = 0;
    stack->cap = 0;
}

static void index_stack_free(RnmIndexStack *stack) {
    free(stack->items);
    stack->items = NULL;
    stack->len = 0;
    stack->cap = 0;
}

static void index_stack_push(RnmIndexStack *stack, size_t value) {
    if (stack->len == stack->cap) {
        size_t new_cap = stack->cap == 0 ? 256U : stack->cap * 2U;
        stack->items = (size_t *) rnm_realloc(stack->items, sizeof(size_t) * new_cap);
        stack->cap = new_cap;
    }
    stack->items[stack->len++] = value;
}

static size_t index_stack_pop(RnmIndexStack *stack) {
    return stack->items[--stack->len];
}

static int is_named_node_modules(const char *name) {
    return rnm_casecmp(name, "node_modules") == 0;
}

static void record_access_error(RnmStats *stats) {
    uint32_t code = rnm_platform_get_last_error_code();
    RnmErrorCategory category = rnm_platform_classify_error(code);

    stats->access_errors++;
    if ((size_t) category < RNM_ERROR_CATEGORY_COUNT) {
        stats->error_categories[(size_t) category]++;
    }
}

static void add_file_bytes(
    RnmScanFrame *frame,
    uint64_t size,
    RnmStats *stats,
    RnmProgressState *progress,
    const RnmScanOptions *options,
    const char *label
) {
    if (!frame->inside_node_modules) {
        return;
    }

    frame->subtree_bytes += size;
    stats->visited_files++;
    print_progress(label, stats, progress, options, 0);
}

static int load_unknown_entry_info(const char *parent_path, const char *name, RnmFileInfo *info) {
    char *child_path = rnm_path_join(parent_path, name);
    int ok = rnm_platform_stat_follow(child_path, info);
    free(child_path);
    return ok;
}

static void push_child_frame(
    RnmScanFrameList *frames,
    RnmIndexStack *stack,
    size_t parent_index,
    const char *parent_path,
    const char *name
) {
    RnmScanFrame frame = {
        rnm_path_join(parent_path, name),
        parent_index,
        0,
        RNM_SCAN_ENTER,
        0,
        0,
        (unsigned char) is_named_node_modules(name)
    };

    index_stack_push(stack, frame_list_push(frames, frame));
}

static void scan_volume(
    const RnmVolume *volume,
    RnmOccurrenceList *occurrences,
    RnmStats *stats,
    const RnmScanOptions *options
) {
    RnmScanFrameList frames;
    RnmIndexStack stack;
    RnmIdentitySet visited_identities;
    RnmProgressState progress;
    RnmScanFrame root_frame = {
        rnm_strdup(volume->path),
        RNM_NO_PARENT,
        0,
        RNM_SCAN_ENTER,
        0,
        0,
        (unsigned char) is_named_node_modules(rnm_path_basename(volume->path))
    };

    frame_list_init(&frames);
    index_stack_init(&stack);
    rnm_identity_set_init(&visited_identities);
    progress.last_update_ms = 0;
    progress.frame = 0;

    index_stack_push(&stack, frame_list_push(&frames, root_frame));

    while (stack.len > 0) {
        size_t frame_index = index_stack_pop(&stack);

        if (frames.items[frame_index].stage == RNM_SCAN_ENTER) {
            RnmFileInfo current_info;
            RnmDirIter iter;
            RnmDirEntry entry;
            int parent_inside = 0;
            char *current_path = frames.items[frame_index].path;

            if (!rnm_platform_stat_follow(current_path, &current_info)) {
                record_access_error(stats);
                free(frames.items[frame_index].path);
                frames.items[frame_index].path = NULL;
                continue;
            }

            if (!current_info.is_directory) {
                free(frames.items[frame_index].path);
                frames.items[frame_index].path = NULL;
                continue;
            }

            if (!rnm_identity_set_insert(&visited_identities, current_info.identity)) {
                stats->skipped_duplicates++;
                free(frames.items[frame_index].path);
                frames.items[frame_index].path = NULL;
                continue;
            }

            if (frames.items[frame_index].parent_index != RNM_NO_PARENT) {
                parent_inside = frames.items[frames.items[frame_index].parent_index].inside_node_modules;
            }

            frames.items[frame_index].inside_node_modules =
                (unsigned char) (parent_inside || frames.items[frame_index].named_node_modules);
            frames.items[frame_index].root_node_modules =
                (unsigned char) (frames.items[frame_index].named_node_modules && !parent_inside);
            frames.items[frame_index].stage = RNM_SCAN_LEAVE;
            index_stack_push(&stack, frame_index);

            if (!rnm_dir_iter_open(current_path, &iter)) {
                record_access_error(stats);
                continue;
            }

            while (rnm_dir_iter_next(&iter, &entry)) {
                if (rnm_path_is_dot_or_dotdot(entry.name)) {
                    continue;
                }

                if (entry.type == RNM_DIRENT_DIRECTORY) {
                    push_child_frame(&frames, &stack, frame_index, current_path, entry.name);
                    continue;
                }

                if (entry.type == RNM_DIRENT_FILE) {
                    if (frames.items[frame_index].inside_node_modules) {
                        if (entry.size_known) {
                            add_file_bytes(&frames.items[frame_index], entry.size, stats, &progress, options, volume->label);
                        } else {
                            RnmFileInfo child_info;
                            if (!load_unknown_entry_info(current_path, entry.name, &child_info)) {
                                record_access_error(stats);
                                continue;
                            }
                            if (child_info.is_directory) {
                                push_child_frame(&frames, &stack, frame_index, current_path, entry.name);
                            } else {
                                add_file_bytes(&frames.items[frame_index], child_info.size, stats, &progress, options, volume->label);
                            }
                        }
                    }
                    continue;
                }

                {
                    RnmFileInfo child_info;
                    if (!load_unknown_entry_info(current_path, entry.name, &child_info)) {
                        record_access_error(stats);
                        continue;
                    }

                    if (child_info.is_directory) {
                        push_child_frame(&frames, &stack, frame_index, current_path, entry.name);
                    } else if (frames.items[frame_index].inside_node_modules) {
                        add_file_bytes(&frames.items[frame_index], child_info.size, stats, &progress, options, volume->label);
                    }
                }
            }

            rnm_dir_iter_close(&iter);
            continue;
        }

        stats->visited_directories++;

        if (frames.items[frame_index].root_node_modules) {
            RnmOccurrence occurrence = {
                rnm_strdup(volume->label),
                rnm_strdup(frames.items[frame_index].path),
                frames.items[frame_index].subtree_bytes
            };

            rnm_occurrence_list_push(occurrences, occurrence);
            stats->node_modules_found++;
            stats->total_node_modules_bytes += frames.items[frame_index].subtree_bytes;
        }

        if (frames.items[frame_index].parent_index != RNM_NO_PARENT &&
            frames.items[frames.items[frame_index].parent_index].inside_node_modules) {
            frames.items[frames.items[frame_index].parent_index].subtree_bytes +=
                frames.items[frame_index].subtree_bytes;
        }

        print_progress(volume->label, stats, &progress, options, 0);
        free(frames.items[frame_index].path);
        frames.items[frame_index].path = NULL;
    }

    print_progress(volume->label, stats, &progress, options, 1);
    rnm_identity_set_free(&visited_identities);
    index_stack_free(&stack);
    frame_list_free(&frames);
}

int rnm_scan_volumes(
    const RnmVolumeList *selected_volumes,
    RnmOccurrenceList *occurrences,
    RnmStats *stats,
    const RnmScanOptions *options
) {
    size_t index;

    for (index = 0; index < selected_volumes->len; ++index) {
        scan_volume(&selected_volumes->items[index], occurrences, stats, options);
        if (options != NULL && options->show_progress) {
            putchar('\n');
        }
    }

    return 1;
}
