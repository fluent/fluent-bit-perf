/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Fluent Bit
 *  ==========
 *  Copyright (C) 2019      The Fluent Bit Authors
 *  Copyright (C) 2015-2018 Treasure Data Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef FLB_REPORT_H
#define FLB_REPORT_H

#define FLB_REPORT_TXT       0
#define FLB_REPORT_MARKDOWN  1
#define FLB_REPORT_CSV       2

#include "flb_proc.h"

struct flb_report {
    int format;          /* report output format */
    int fd;              /* report file descriptor */
    int cpu_count;       /* number of CPUs */
    int cpu_ticks;       /* cpu clock ticks */
    int snapshots;       /* number of stats snapshots */
    int wait_time;       /* monitoring wait time */

    /* Process info */
    pid_t pid;           /* monitored process ID */
    char *name;          /* process name */

    /* General stats */
    size_t sum_bytes;    /* Total number of bytes */
    size_t sum_mem;      /* total number of bytes reported per snapshot */
    size_t sum_records;  /* total number of log records */
    int sum_cpu_count;   /* CPU snapshots summarized */
    double sum_cpu;      /* total %CPU usage */
    double sum_duration; /* total elapsed time of tests */

};

struct flb_report *flb_report_create(char *out, int format, int pid, int wait);
int flb_report_stats(struct flb_report *r, int records,
                     size_t bytes,
                     struct flb_proc_task *t1, struct flb_proc_task *t2);

char *flb_report_human_readable_size(long size);
double flb_report_cpu_usage(struct flb_report *r,
                            struct flb_proc_task *t1, struct flb_proc_task *t2);

int flb_report_summary(struct flb_report *r);
int flb_report_destroy(struct flb_report *r);

#endif
