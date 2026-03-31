#include "report.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int compare_occurrences_desc(const void *left, const void *right) {
    const RnmOccurrence *lhs = (const RnmOccurrence *) left;
    const RnmOccurrence *rhs = (const RnmOccurrence *) right;

    if (lhs->bytes < rhs->bytes) {
        return 1;
    }
    if (lhs->bytes > rhs->bytes) {
        return -1;
    }
    return strcmp(lhs->path, rhs->path);
}

static uint64_t volume_bytes(const RnmOccurrenceList *occurrences, const char *volume_label) {
    uint64_t total = 0;
    size_t index;

    for (index = 0; index < occurrences->len; ++index) {
        if (strcmp(occurrences->items[index].volume_label, volume_label) == 0) {
            total += occurrences->items[index].bytes;
        }
    }
    return total;
}

static void write_error_breakdown_file(FILE *file, const RnmStats *stats) {
    size_t index;

    fprintf(file, "Errors: %" PRIu64 "\n", stats->access_errors);
    for (index = 0; index < RNM_ERROR_CATEGORY_COUNT; ++index) {
        if (stats->error_categories[index] == 0) {
            continue;
        }

        fprintf(
            file,
            "  %s: %" PRIu64 "\n",
            rnm_platform_error_category_name((RnmErrorCategory) index),
            stats->error_categories[index]
        );
    }
}

static void print_error_breakdown_console(const RnmStats *stats) {
    size_t index;

    printf("Errors: %" PRIu64 "\n", stats->access_errors);
    for (index = 0; index < RNM_ERROR_CATEGORY_COUNT; ++index) {
        if (stats->error_categories[index] == 0) {
            continue;
        }

        printf(
            "  %s: %" PRIu64 "\n",
            rnm_platform_error_category_name((RnmErrorCategory) index),
            stats->error_categories[index]
        );
    }
}

int rnm_write_report(
    const char *report_path,
    const RnmOccurrenceList *occurrences,
    const RnmVolumeList *selected_volumes,
    const RnmStats *stats,
    const RnmRunTimings *timings
) {
    FILE *file;
    size_t index;
    char bytes_buffer[32];
    RnmOccurrence *copy = NULL;

    file = fopen(report_path, "w");
    if (file == NULL) {
        return 0;
    }

    if (occurrences->len > 0) {
        copy = (RnmOccurrence *) rnm_malloc(sizeof(RnmOccurrence) * occurrences->len);
        memcpy(copy, occurrences->items, sizeof(RnmOccurrence) * occurrences->len);
        qsort(copy, occurrences->len, sizeof(RnmOccurrence), compare_occurrences_desc);
    }

    fprintf(file, "rip-node_modules report\n");
    fprintf(file, "=======================\n\n");
    fprintf(file, "Volumes scanned: %zu\n", selected_volumes->len);
    fprintf(file, "node_modules found: %" PRIu64 "\n", stats->node_modules_found);
    fprintf(file, "Total bytes: %" PRIu64 " (%s)\n", stats->total_node_modules_bytes,
        rnm_format_bytes(stats->total_node_modules_bytes, bytes_buffer, sizeof(bytes_buffer)));
    fprintf(file, "Visited directories: %" PRIu64 "\n", stats->visited_directories);
    fprintf(file, "Visited files: %" PRIu64 "\n", stats->visited_files);
    fprintf(file, "Skipped duplicates: %" PRIu64 "\n", stats->skipped_duplicates);
    write_error_breakdown_file(file, stats);
    fprintf(file, "Elapsed time: %" PRIu64 " ms\n", timings->total_ms);
    fprintf(file, "  scan: %" PRIu64 " ms\n", timings->scan_ms);
    fprintf(file, "  report: %" PRIu64 " ms\n", timings->report_ms);
    fprintf(file, "\n");

    for (index = 0; index < selected_volumes->len; ++index) {
        uint64_t total = volume_bytes(occurrences, selected_volumes->items[index].label);
        fprintf(file, "[%s] %s\n", selected_volumes->items[index].label,
            rnm_format_bytes(total, bytes_buffer, sizeof(bytes_buffer)));
    }

    fprintf(file, "\nOccurrences\n");
    fprintf(file, "-----------\n");

    for (index = 0; index < occurrences->len; ++index) {
        fprintf(
            file,
            "%s | %" PRIu64 " | %s | %s\n",
            copy[index].volume_label,
            copy[index].bytes,
            rnm_format_bytes(copy[index].bytes, bytes_buffer, sizeof(bytes_buffer)),
            copy[index].path
        );
    }

    free(copy);
    fclose(file);
    return 1;
}

void rnm_print_summary(
    const RnmOccurrenceList *occurrences,
    const RnmVolumeList *selected_volumes,
    const RnmStats *stats,
    const RnmRunTimings *timings,
    const char *report_path
) {
    size_t index;
    char bytes_buffer[32];
    RnmOccurrence *copy = NULL;

    printf("\nScan finished.\n");
    printf("Volumes scanned: %zu\n", selected_volumes->len);
    printf("node_modules found: %" PRIu64 "\n", stats->node_modules_found);
    printf("Total impact: %s\n",
        rnm_format_bytes(stats->total_node_modules_bytes, bytes_buffer, sizeof(bytes_buffer)));
    printf("Visited directories: %" PRIu64 "\n", stats->visited_directories);
    printf("Skipped duplicates: %" PRIu64 "\n", stats->skipped_duplicates);
    print_error_breakdown_console(stats);
    printf("Elapsed time: %" PRIu64 " ms\n", timings->total_ms);
    printf("  scan: %" PRIu64 " ms\n", timings->scan_ms);
    printf("  report: %" PRIu64 " ms\n", timings->report_ms);

    for (index = 0; index < selected_volumes->len; ++index) {
        uint64_t total = volume_bytes(occurrences, selected_volumes->items[index].label);
        printf(
            "  %s: %s\n",
            selected_volumes->items[index].label,
            rnm_format_bytes(total, bytes_buffer, sizeof(bytes_buffer))
        );
    }

    if (occurrences->len > 0) {
        size_t limit = occurrences->len < 10 ? occurrences->len : 10;
        copy = (RnmOccurrence *) rnm_malloc(sizeof(RnmOccurrence) * occurrences->len);
        memcpy(copy, occurrences->items, sizeof(RnmOccurrence) * occurrences->len);
        qsort(copy, occurrences->len, sizeof(RnmOccurrence), compare_occurrences_desc);

        puts("\nTop node_modules:");
        for (index = 0; index < limit; ++index) {
            printf(
                "  %s | %s | %s\n",
                copy[index].volume_label,
                rnm_format_bytes(copy[index].bytes, bytes_buffer, sizeof(bytes_buffer)),
                copy[index].path
            );
        }

        free(copy);
    }

    printf("\nReport: %s\n", report_path);
}
