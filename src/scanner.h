#ifndef RIP_NODE_MODULES_SCANNER_H
#define RIP_NODE_MODULES_SCANNER_H

#include "common.h"

typedef struct {
    int show_progress;
} RnmScanOptions;

int rnm_scan_volumes(
    const RnmVolumeList *selected_volumes,
    RnmOccurrenceList *occurrences,
    RnmStats *stats,
    const RnmScanOptions *options
);

#endif
