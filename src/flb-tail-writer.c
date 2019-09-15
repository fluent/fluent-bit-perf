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
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <time.h>

/* local headers */
#include "mk_list.h"
#include "flb_data_file.h"
#include "flb_proc.h"
#include "flb_report.h"

/* Default values */
#define DEFAULT_RECORDS    1000  /* 1000 records per second */
#define DEFAULT_INC_BY        0  /* no increase             */
#define DEFAULT_SECONDS      10  /* test time: 10 seconds   */

static int flb_help(int rc)
{
    printf("Usage: flb-tail-writer [OPTIONS]\n\n");
    printf("Available options\n");
    printf("  -d, --datafile=PATH\t\tspecify source data file\n");
    printf("  -p  --pid=FLB_PID\t\tFluent Bit PID used gather metrics\n");
    printf("  -o, --output=PATH\t\tset output file name\n");
    printf("  -i, --increase_by=N\t\tincrease N number of records per second (default: %i)\n",
           DEFAULT_INC_BY);
    printf("  -r, --records=RECORDS\t\trecords per second (default: %i)\n",
           DEFAULT_RECORDS);
    printf("  -s, --seconds=SECONDS\t\ttotal test time meassured in seconds (default: %i)\n",
           DEFAULT_SECONDS);
    printf("  -R, --report\t\t\tset report output file (default: stdout)\n");
    printf("  -F, --format\t\t\treport format, text (default) or markdown)\n");
    printf("  -h, --help\t\t\tprint this help");
    printf("\n\n");
    exit(rc);
}

static void close_report(int fd)
{
    if (fd == STDOUT_FILENO) {
        return;
    }

    if (fd != -1) {
        close(fd);
    }
}

static int run_fs_writer(pid_t pid,
                         char *report,
                         int fmt_report,
                         char *in_data_file, char *out_data_file,
                         int records, int increase_by,
                         int seconds)
{
    int i;
    int x;
    int in_fd;
    int out_fd;
    int ret;
    int round_records;
    int report_fd = -1;
    int wait_time = 3;
    size_t round_bytes;
    off_t off = 0;
    off_t off_rec;
    off_t off_inc;
    char *data_buf;
    size_t data_size;
    ssize_t bytes;
    ssize_t total_cpu = 0;
    ssize_t total_mem = 0;
    size_t total_records = 0;
    ssize_t total_bytes = 0;
    char *proc_name = NULL;
    struct flb_proc_task *t1;
    struct flb_proc_task *t2;
    struct flb_report *r = NULL;
    time_t start_time;
    time_t end_time;

    /* Report file for process monitoring */
    if (pid >= 0) {
        r = flb_report_create(report, fmt_report, pid, wait_time);
        if (!r) {
            fprintf(stderr, "error: cannot initialize report");
            return -1;
        }
    }

    /* Open output target file */
    out_fd = open(out_data_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (out_fd == -1) {
        perror("open");
        fprintf(stderr, "error: cannot open/create output data file '%s'\n",
                out_data_file);
        if (r) {
            flb_report_destroy(r);
        }
        return -1;
    }

    /* Load input data file in-memory */
    in_fd = flb_data_file_load(in_data_file, &data_buf, &data_size);
    if (in_fd == -1) {
        close(out_fd);
        fprintf(stderr, "error: cannot load input data file '%s'\n",
                in_data_file);
        if (r) {
            flb_report_destroy(r);
        }
        return -1;
    }

    /* Retrieve the final offset of 'records' */
    ret = flb_data_file_offset_records(records, data_buf, data_size, &off_rec);
    if (ret == -1) {
        fprintf(stderr, "error: cannot find %i number of records\n", records);
        flb_data_file_unload(data_buf, data_size);
        close(in_fd);
        close(out_fd);
        if (r) {
            flb_report_destroy(r);
        }
        return -1;
    }

    /* Retrieve the final offset of 'records' */
    ret = flb_data_file_offset_records(increase_by, data_buf, data_size,
                                       &off_inc);
    if (ret == -1) {
        fprintf(stderr, "error: cannot find %i number of records\n", records);
        flb_data_file_unload(data_buf, data_size);
        close(in_fd);
        close(out_fd);
        if (r) {
            flb_report_destroy(r);
        }
        return -1;
    }

    /* Get Process name */
    if (pid >= 0) {
        t1 = flb_proc_stat_create(pid);
        proc_name = strndup(t1->name + 1, strlen(t1->name) - 2);
    }

    /* Set the start time */
    start_time = time(NULL);

    /*
     * Just write data chunks every second. Since a write operation will
     * put the data into a kernel buffer (or zero copy) and likely return
     * immediately, we don't care about the time invested in that operation,
     * just the wait time between writes.
     */
    for (i = 0; i < seconds; i++) {
        round_bytes = 0;
        round_records = 0;

        if (pid >= 0) {
            t1 = flb_proc_stat_create(pid);
            if (!t1) {
                fprintf(stderr, "error gathering stats for PID %i\n",
                        (int) pid);
            }
        }

        /*
         * Use zero-copy strategy with sendfile(2). In benchmarking we want
         * to avoid extra Kernel work, this is a Linux specific feature.
         */
        off = 0;
        /* Dispatch the records chunk */
        bytes = sendfile(out_fd, in_fd, &off, off_rec);
        if (bytes == -1) {
            perror("sendfile");
            fprintf(stderr, "error: exception on writing records chunk\n");
        }
        else {
            total_bytes += bytes;
            total_records += records;
            round_records = records;
            round_bytes = bytes;
        }

        if (increase_by > 0 && i > 0) {
            /*
             * Send incremental records, yeah, this involve a second system
             * call but it's better than writev() and data-copy.
             */
            for (x = 0; x < i; x++) {
                off = 0;
                bytes = sendfile(out_fd, in_fd, &off, off_inc);
                if (bytes == -1) {
                    perror("sendfile");
                    fprintf(stderr, "error: cannot write inc records chunk\n");
                }
                else {
                    total_bytes += bytes;
                    total_records += increase_by;
                    round_records += increase_by;
                    round_bytes += bytes;
                }
            }
        }
        /* Dummy sleep */
        sleep(1);

        /* Get stats */
        if (pid >= 0) {
            t2 = flb_proc_stat_create(pid);
            if (!t2) {
                fprintf(stderr, "error gathering stats for PID %i\n",
                        (int) pid);
            }

            if (r) {
                flb_report_stats(r, round_records, round_bytes, t1, t2);
            }

            /* Count total number of bytes in memory */
            total_cpu += flb_report_cpu_usage(r, t1, t2);
            total_mem += t2->r_rss;

            flb_proc_stat_destroy(t1);
            flb_proc_stat_destroy(t2);
        }
    }

    /*
     * Create continuos snapshots until resources consumption (CPU) stabilize,
     * we assume that after two seconds without deltas in user time the process
     * finished processing our records.
     */
    if (pid >= 0) {
        int count = 0;
        int test_time;
        char *tmp;

        while (1) {
            t1 = flb_proc_stat_create(pid);
            sleep(1);
            t2 = flb_proc_stat_create(pid);
            flb_report_stats(r, 0, 0, t1, t2);

            if ((t2->r_utime_ms - t1->r_utime_ms) == 0) {
                count++;
            }
            else {
                if (count > 0) {
                    count = 0;
                }
            }


            if (count >= wait_time) {
                flb_proc_stat_destroy(t1);
                flb_proc_stat_destroy(t2);
                break;
            }

            flb_proc_stat_destroy(t1);
            flb_proc_stat_destroy(t2);

        }

        flb_report_summary(r);
    }

    if (r) {
        flb_report_destroy(r);
    }
}

int main(int argc, char **argv)
{
    int ret;
    int opt;
    int records = DEFAULT_RECORDS;
    int seconds = DEFAULT_SECONDS;
    int increase_by = DEFAULT_INC_BY;
    int out_fd;
    int pid = -1;
    int fd_report;
    int fmt_report = FLB_REPORT_TXT;
    char *format = NULL;
    char *report = NULL;
    char *out_file = NULL;
    char *data_file = NULL;

    /* Setup long-options */
    static const struct option long_opts[] = {
        { "datafile"   ,   required_argument, NULL, 'd' },
        { "pid"        ,   required_argument, NULL, 'p' },
        { "output"     ,   required_argument, NULL, 'o' },
        { "records"    ,   required_argument, NULL, 'r' },
        { "increase_by",   required_argument, NULL, 'i' },
        { "seconds"    ,   required_argument, NULL, 's' },
        { "report"     ,   required_argument, NULL, 'R' },
        { "format"     ,   required_argument, NULL, 'F' },
        { "help"       ,   no_argument      , NULL, 'h' },
    };

    while ((opt = getopt_long(argc, argv,
                              "d:p:o:r:i:s:R:F:h", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'd':
            data_file = strdup(optarg);
            break;
        case 'p':
            pid = atoi(optarg);
            break;
        case 'o':
            out_file = strdup(optarg);
            break;
        case 'r':
            records = atoi(optarg);
            break;
        case 'i':
            increase_by = atoi(optarg);
            break;
        case 's':
            seconds = atoi(optarg);
            break;
        case 'R':
            report = strdup(optarg);
            break;
        case 'F':
            format = strdup(optarg);
            break;
        case 'h':
            flb_help(EXIT_SUCCESS);
            break;
        };
    };

    if (!data_file) {
        fprintf(stderr, "error: no data file specified\n");
        exit(EXIT_FAILURE);
    }

    if (records < 1) {
        fprintf(stderr, "error: invalid number of records '%i'\n", records);
        exit(EXIT_FAILURE);
    }

    if (seconds < 1) {
        fprintf(stderr, "error: invalid number of seconds '%i'\n", seconds);
        exit(EXIT_FAILURE);
    }

    if (!out_file) {
        fprintf(stderr, "error: no output data file defined\n");
        exit(EXIT_FAILURE);
    }

    if (format) {
        if (strcasecmp(format, "markdown") == 0) {
            fmt_report = FLB_REPORT_MARKDOWN;
        }
        else if (strcasecmp(format, "text") == 0) {
            fmt_report = FLB_REPORT_TXT;
        }
        else {
            fprintf(stderr, "error: invalid format type");
            exit(EXIT_FAILURE);
        }
    }

    ret = run_fs_writer(pid, report, fmt_report, data_file, out_file,
                        records, increase_by, seconds);
    if (ret == -1) {
        exit(EXIT_FAILURE);
    }

    return 0;
}
