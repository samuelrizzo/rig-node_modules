#ifndef RIP_NODE_MODULES_REPORT_H
#define RIP_NODE_MODULES_REPORT_H

#include "common.h"

int rnm_write_report(
    const char *report_path,
    const RnmOccurrenceList *occurrences,
    const RnmVolumeList *selected_volumes,
    const RnmStats *stats,
    const RnmRunTimings *timings
);

void rnm_print_summary(
    const RnmOccurrenceList *occurrences,
    const RnmVolumeList *selected_volumes,
    const RnmStats *stats,
    const RnmRunTimings *timings,
    const char *report_path
);

#endif
