#include "common.h"
#include "platform.h"
#include "report.h"
#include "scanner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define RNM_STRTOK strtok_s
#else
#define RNM_STRTOK strtok_r
#endif

typedef struct {
    const char *scan_path;
    const char *report_path;
    const char *benchmark_out;
    int show_progress;
} RnmCliOptions;

static uint64_t now_ms(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return ((uint64_t) ts.tv_sec * 1000U) + ((uint64_t) ts.tv_nsec / 1000000U);
}

static void print_usage(const char *program_name) {
    printf("Usage:\n");
    printf("  %s\n", program_name);
    printf("  %s --scan-path <path> [--report-path <file>] [--benchmark-out <file>] [--no-progress]\n", program_name);
}

static int parse_cli_options(int argc, char **argv, RnmCliOptions *options) {
    int index;

    options->scan_path = NULL;
    options->report_path = "node_modules_report.txt";
    options->benchmark_out = NULL;
    options->show_progress = 1;

    for (index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--scan-path") == 0) {
            if (index + 1 >= argc) {
                return 0;
            }
            options->scan_path = argv[++index];
        } else if (strcmp(argv[index], "--report-path") == 0) {
            if (index + 1 >= argc) {
                return 0;
            }
            options->report_path = argv[++index];
        } else if (strcmp(argv[index], "--benchmark-out") == 0) {
            if (index + 1 >= argc) {
                return 0;
            }
            options->benchmark_out = argv[++index];
        } else if (strcmp(argv[index], "--no-progress") == 0) {
            options->show_progress = 0;
        } else if (strcmp(argv[index], "--help") == 0 || strcmp(argv[index], "-h") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else {
            return 0;
        }
    }

    return 1;
}

static int write_benchmark_results(
    const char *path,
    const RnmCliOptions *options,
    const RnmStats *stats,
    const RnmRunTimings *timings
) {
    FILE *file = fopen(path, "w");
    char bytes_buffer[32];
    size_t index;

    if (file == NULL) {
        return 0;
    }

    fprintf(file, "scan_path=%s\n", options->scan_path == NULL ? "(interactive)" : options->scan_path);
    fprintf(file, "report_path=%s\n", options->report_path);
    fprintf(file, "progress=%s\n", options->show_progress ? "on" : "off");
    fprintf(file, "scan_ms=%" PRIu64 "\n", timings->scan_ms);
    fprintf(file, "report_ms=%" PRIu64 "\n", timings->report_ms);
    fprintf(file, "total_ms=%" PRIu64 "\n", timings->total_ms);
    fprintf(file, "visited_directories=%" PRIu64 "\n", stats->visited_directories);
    fprintf(file, "visited_files=%" PRIu64 "\n", stats->visited_files);
    fprintf(file, "skipped_duplicates=%" PRIu64 "\n", stats->skipped_duplicates);
    fprintf(file, "access_errors=%" PRIu64 "\n", stats->access_errors);
    fprintf(file, "node_modules_found=%" PRIu64 "\n", stats->node_modules_found);
    fprintf(file, "total_node_modules_bytes=%" PRIu64 "\n", stats->total_node_modules_bytes);
    fprintf(file, "total_node_modules_human=%s\n",
        rnm_format_bytes(stats->total_node_modules_bytes, bytes_buffer, sizeof(bytes_buffer)));
    for (index = 0; index < RNM_ERROR_CATEGORY_COUNT; ++index) {
        fprintf(
            file,
            "error_category_%s=%" PRIu64 "\n",
            rnm_platform_error_category_name((RnmErrorCategory) index),
            stats->error_categories[index]
        );
    }

    fclose(file);
    return 1;
}

static void print_volume_menu(const RnmVolumeList *volumes) {
    size_t index;
    char size_buffer[32];
    size_t custom_option = volumes->len + 1U;

    puts("Active volumes:");
    for (index = 0; index < volumes->len; ++index) {
        printf("  %zu. %s (%s)\n", index + 1, volumes->items[index].label,
            rnm_format_bytes(volumes->items[index].total_bytes, size_buffer, sizeof(size_buffer)));
    }
    printf("  %zu. Type a directory path manually\n", custom_option);
    puts("Type one or more disk numbers separated by comma, 'all', or the custom option number.");
}

static int parse_selection(const char *input, const RnmVolumeList *available, RnmVolumeList *selected) {
    char *copy;
    char *token;
    char *context = NULL;
    size_t index;

    if (rnm_casecmp(input, "all") == 0) {
        for (index = 0; index < available->len; ++index) {
            RnmVolume volume = {
                rnm_strdup(available->items[index].path),
                rnm_strdup(available->items[index].label),
                available->items[index].total_bytes
            };
            rnm_volume_list_push(selected, volume);
        }
        return selected->len > 0;
    }

    copy = rnm_strdup(input);
    token = RNM_STRTOK(copy, ", ", &context);
    while (token != NULL) {
        char *end = NULL;
        long parsed = strtol(token, &end, 10);
        if (end == token || *end != '\0' || parsed <= 0 || (size_t) parsed > available->len) {
            free(copy);
            return 0;
        }

        for (index = 0; index < selected->len; ++index) {
            if (strcmp(selected->items[index].path, available->items[(size_t) parsed - 1].path) == 0) {
                token = RNM_STRTOK(NULL, ", ", &context);
                goto next_token;
            }
        }

        {
            size_t source_index = (size_t) parsed - 1;
            RnmVolume volume = {
                rnm_strdup(available->items[source_index].path),
                rnm_strdup(available->items[source_index].label),
                available->items[source_index].total_bytes
            };
            rnm_volume_list_push(selected, volume);
        }

next_token:
        token = RNM_STRTOK(NULL, ", ", &context);
    }

    free(copy);
    return selected->len > 0;
}

static int parse_custom_selection(const char *input, const RnmVolumeList *available) {
    char *end = NULL;
    long parsed = strtol(input, &end, 10);

    if (end == input || *end != '\0') {
        return 0;
    }

    return (size_t) parsed == available->len + 1U;
}

static int add_custom_directory(const char *path, RnmVolumeList *selected) {
    RnmFileInfo info;
    uint64_t total_bytes = 0;

    if (!rnm_platform_stat_follow(path, &info) || !info.exists || !info.is_directory) {
        return 0;
    }

    rnm_volume_list_push(selected, (RnmVolume) {
        rnm_strdup(path),
        rnm_strdup(path),
        rnm_volume_total_bytes(path, &total_bytes) ? total_bytes : 0
    });
    return 1;
}

static int prompt_custom_directory(RnmVolumeList *selected) {
    char buffer[1024];

    while (1) {
        puts("Type the directory path to scan:");
        printf("> ");

        if (fgets(buffer, (int) sizeof(buffer), stdin) == NULL) {
            return 0;
        }

        buffer[strcspn(buffer, "\r\n")] = '\0';
        if (buffer[0] == '\0') {
            continue;
        }

        if (add_custom_directory(buffer, selected)) {
            return 1;
        }

        puts("Invalid directory. Try again.");
    }
}

static int prompt_volume_selection(const RnmVolumeList *available, RnmVolumeList *selected) {
    char buffer[256];

    while (1) {
        print_volume_menu(available);
        printf("> ");

        if (fgets(buffer, (int) sizeof(buffer), stdin) == NULL) {
            return 0;
        }

        buffer[strcspn(buffer, "\r\n")] = '\0';
        if (buffer[0] == '\0') {
            continue;
        }

        if (parse_custom_selection(buffer, available)) {
            return prompt_custom_directory(selected);
        }

        if (parse_selection(buffer, available, selected)) {
            return 1;
        }

        puts("Invalid selection. Try again.");
        rnm_volume_list_free(selected);
        rnm_volume_list_init(selected);
    }
}

int main(int argc, char **argv) {
    RnmVolumeList available;
    RnmVolumeList selected;
    RnmOccurrenceList occurrences;
    RnmStats stats;
    RnmRunTimings timings;
    RnmCliOptions options;
    RnmScanOptions scan_options;
    uint64_t total_start_ms;
    uint64_t scan_start_ms;
    uint64_t report_start_ms;

    if (!parse_cli_options(argc, argv, &options)) {
        print_usage(argv[0]);
        return 1;
    }

    scan_options.show_progress = options.show_progress;
    memset(&stats, 0, sizeof(stats));
    memset(&timings, 0, sizeof(timings));
    rnm_volume_list_init(&available);
    rnm_volume_list_init(&selected);
    rnm_occurrence_list_init(&occurrences);

    if (options.scan_path != NULL) {
        uint64_t total_bytes = 0;
        rnm_volume_list_push(&selected, (RnmVolume) {
            rnm_strdup(options.scan_path),
            rnm_strdup(options.scan_path),
            rnm_volume_total_bytes(options.scan_path, &total_bytes) ? total_bytes : 0
        });
    } else {
        if (!rnm_platform_list_volumes(&available) || available.len == 0) {
            fprintf(stderr, "No active volumes found.\n");
            return 1;
        }

        if (!prompt_volume_selection(&available, &selected)) {
            fprintf(stderr, "Volume selection failed.\n");
            rnm_platform_free_volumes(&available);
            return 1;
        }
    }

    total_start_ms = now_ms();
    scan_start_ms = now_ms();
    if (!rnm_scan_volumes(&selected, &occurrences, &stats, &scan_options)) {
        fprintf(stderr, "Scan failed.\n");
        rnm_platform_free_volumes(&available);
        rnm_volume_list_free(&selected);
        return 1;
    }
    timings.scan_ms = now_ms() - scan_start_ms;

    report_start_ms = now_ms();
    timings.total_ms = now_ms() - total_start_ms;
    if (!rnm_write_report(options.report_path, &occurrences, &selected, &stats, &timings)) {
        fprintf(stderr, "Failed to write report to %s.\n", options.report_path);
        rnm_platform_free_volumes(&available);
        rnm_volume_list_free(&selected);
        rnm_occurrence_list_free(&occurrences);
        return 1;
    }
    timings.report_ms = now_ms() - report_start_ms;
    timings.total_ms = now_ms() - total_start_ms;

    if (!rnm_write_report(options.report_path, &occurrences, &selected, &stats, &timings)) {
        fprintf(stderr, "Failed to finalize report at %s.\n", options.report_path);
        rnm_platform_free_volumes(&available);
        rnm_volume_list_free(&selected);
        rnm_occurrence_list_free(&occurrences);
        return 1;
    }

    rnm_print_summary(&occurrences, &selected, &stats, &timings, options.report_path);

    if (options.benchmark_out != NULL) {
        if (!write_benchmark_results(options.benchmark_out, &options, &stats, &timings)) {
            fprintf(stderr, "Failed to write benchmark results to %s.\n", options.benchmark_out);
            rnm_platform_free_volumes(&available);
            rnm_volume_list_free(&selected);
            rnm_occurrence_list_free(&occurrences);
            return 1;
        }
        printf("Benchmark: %s\n", options.benchmark_out);
    }

    rnm_platform_free_volumes(&available);
    rnm_volume_list_free(&selected);
    rnm_occurrence_list_free(&occurrences);
    return 0;
}
