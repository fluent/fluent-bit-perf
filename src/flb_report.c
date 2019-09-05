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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "flb_proc.h"
#include "flb_report.h"

char *flb_report_human_readable_size(long size)
{
    long u = 1024, i, len = 128;
    char *buf = malloc(len);
    static const char *__units[] = { "b", "K", "M", "G",
        "T", "P", "E", "Z", "Y", NULL
    };

    for (i = 0; __units[i] != NULL; i++) {
        if ((size / u) == 0) {
            break;
        }
        u *= 1024;
    }
    if (!i) {
        snprintf(buf, len, "%ld %s", size, __units[0]);
    }
    else {
        float fsize = (float) ((double) size / (u / 1024));
        snprintf(buf, len, "%.2f%s", fsize, __units[i]);
    }

    return buf;
}

static void report_txt_header(struct flb_report *r)
{
    dprintf(r->fd,
            " records  write (bytes)     write |  %% cpu  user time (ms)  "
            "system time (ms)  Mem (bytes)      Mem\n");
    dprintf(r->fd,
            "--------  -------------  -------- + ------  --------------  "
            "----------------  -----------  -------\n");
}

static void report_markdown_header(struct flb_report *r)
{
    dprintf(r->fd,
            "| records | write (bytes) | write | %%cpu | user time (ms) "
            "| system time (ms) | Mem (bytes) | Mem |\n");
    dprintf(r->fd,
            "|    ---: |          ---: |  ---: |  ---: |        ---: "
            "|             ---: |        ---: |---: |\n");
}

struct flb_report *flb_report_create(char *out, int format)
{
    int fd;
    char *target;
    struct flb_report *r;

    if (out) {
        fd = open(out, O_CREAT | O_TRUNC | O_WRONLY, 0666);
        if (fd == -1) {
            perror("open");
            fprintf(stderr, "error: cannot open/create report file '%s'\n",
                    out);
            return NULL;
        }
    }
    else {
        fd = STDOUT_FILENO;
    }

    r = malloc(sizeof(struct flb_report));
    if (!r) {
        perror("malloc");
        close(fd);
        return NULL;
    }
    r->fd = fd;
    r->format = format;
    r->cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    r->cpu_ticks = sysconf(_SC_CLK_TCK);

    if (r->format == FLB_REPORT_TXT) {
        report_txt_header(r);
    }
    else if (r->format == FLB_REPORT_MARKDOWN) {
        report_markdown_header(r);
    }

    return r;
}

double flb_report_cpu_usage(struct flb_report *r,
                            struct flb_proc_task *t1, struct flb_proc_task *t2)
{
    double diff;
    double cpu;

    /* Calculate CPU usage */
    diff = ((t2->utime + t2->stime) - (t1->utime + t1->stime));
    cpu = ((diff * 100.0) / r->cpu_ticks);

    return cpu;
}

int flb_report_stats(struct flb_report *r, int records,
                     size_t bytes,
                     struct flb_proc_task *t1, struct flb_proc_task *t2)
{
    char *rss_hr;
    char *bytes_hr;
    double cpu;

    cpu = flb_report_cpu_usage(r, t1, t2);

    /* Convert bytes to human readable size */
    rss_hr =  flb_report_human_readable_size(t2->r_rss);
    bytes_hr = flb_report_human_readable_size(bytes);

    if (r->format == FLB_REPORT_TXT) {
        dprintf(r->fd, "%8d  %13zu  %8s | %6.2lf  %14ld  %16ld %12ld %8s\n",
                records,
                bytes,
                bytes_hr,
                cpu,
                (t2->r_utime_ms - t1->r_utime_ms),
                (t2->r_stime_ms - t1->r_stime_ms),
                t2->r_rss,
                rss_hr);
    }
    else if (r->format == FLB_REPORT_MARKDOWN) {
        dprintf(r->fd, "| %d | %zu | %s | %.2lf | %ld | %ld | %ld | %s |\n",
                records,
                bytes,
                bytes_hr,
                cpu,
                (t2->r_utime_ms - t1->r_utime_ms),
                (t2->r_stime_ms - t1->r_stime_ms),
                t2->r_rss,
                rss_hr);
    }

    free(rss_hr);
    free(bytes_hr);
}

int flb_report_destroy(struct flb_report *r)
{
    if (r->fd != STDOUT_FILENO) {
        close(r->fd);
    }
    free(r);
    return 0;
}
