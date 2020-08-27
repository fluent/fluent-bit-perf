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
#include "flb_network.h"

/* Default values */
#define DEFAULT_RECORDS           1000  /* 1000 records per second */
#define DEFAULT_INC_BY               0  /* no increase             */
#define DEFAULT_SECONDS             10  /* test time: 10 seconds   */
#define DEFAULT_CONCURRENCY          1  /* one active connection   */

/* Default network host and port */
#define DEFAULT_PORT            "5170"
#define DEFAULT_HOST       "127.0.0.1"

struct tcp_conn {
    int fd;
    struct mk_list _head;
};

static void tcp_connect_destroy(struct mk_list *list)
{
    struct mk_list *tmp;
    struct mk_list *head;
    struct tcp_conn *conn;

    mk_list_foreach_safe(head, tmp, list) {
        conn = mk_list_entry(head, struct tcp_conn, _head);
        mk_list_del(&conn->_head);
        if (conn->fd > 0) {
            close(conn->fd);
        }
        free(conn);
    }

    free(list);
}

static struct mk_list *tcp_connect_create(int connections, char *host, char *port)
{
    int i;
    int fd;
    struct mk_list *list;
    struct tcp_conn *conn;

    list = malloc(sizeof(struct mk_list));
    if (!list) {
        perror("malloc");
        return NULL;
    }
    mk_list_init(list);

    for (i = 0; i < connections; i++) {
        conn = calloc(1, sizeof(struct tcp_conn));
        if (!conn) {
            perror("calloc");
            fprintf(stderr, "error creating connection #%i to %s:%s\n",
                    i, host, port);
            tcp_connect_destroy(list);
            return NULL;
        }

        /* Perform TCP connection */
        fd = flb_net_tcp_connect(host, port);
        if (fd == -1) {
            fprintf(stderr, "error creating connection #%i to %s:%s\n",
                    i, host, port);
            tcp_connect_destroy(list);
            return NULL;
        }

        conn->fd = fd;
        mk_list_add(&conn->_head, list);
    }

    return list;
}

static int flb_help(int rc)
{
    printf("Usage: flb-tcp-writer [OPTIONS]\n\n");
    printf("Available options\n");
    printf("  -c, --concurrency=N\t\tconcurrency level (default: %i)\n",
           DEFAULT_CONCURRENCY);
    printf("  -d, --datafile=PATH\t\tspecify source data file\n");
    printf("  -p  --pid=FLB_PID\t\tFluent Bit PID used gather metrics\n");
    printf("  -o, --output=HOST:PORT\tset remote TCP Host and Port\n");
    printf("  -i, --increase_by=N\t\tincrease N number of records per second (default: %i)\n",
           DEFAULT_INC_BY);
    printf("  -r, --records=RECORDS\t\trecords per second (default: %i)\n",
           DEFAULT_RECORDS);
    printf("  -s, --seconds=SECONDS\t\ttotal test time meassured in seconds (default: %i)\n",
           DEFAULT_SECONDS);
    printf("  -R, --report\t\t\tset report output file (default: stdout)\n");
    printf("  -F, --format\t\t\treport format: text (default) or markdown\n");
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

static int run_tcp_writer(pid_t pid,
                          char *report,
                          int fmt_report,
                          char *in_data_file,
                          char *host, char *port,
                          int n_cons,
                          int records, int increase_by,
                          int seconds)
{
    int i;
    int x;
    int in_fd;
    int out_fd;
    int ret;
    int conn_records;
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
    double  total_cpu = 0;
    ssize_t total_mem = 0;
    size_t total_records = 0;
    ssize_t total_bytes = 0;
    char *proc_name = NULL;
    struct flb_proc_task *t1;
    struct flb_proc_task *t2;
    struct flb_report *r = NULL;
    struct mk_list *head;
    struct mk_list *connections;
    struct tcp_conn *conn;
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

    /* Create TCP connections */
    connections = tcp_connect_create(n_cons, host, port);
    if (!connections) {
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

    /* Get the number of records that will be send per connection */
    conn_records = (records / n_cons);

    /* Retrieve the final offset of 'records' */
    ret = flb_data_file_offset_records(conn_records, data_buf, data_size, &off_rec);
    if (ret == -1) {
        fprintf(stderr, "error: cannot find %i number of records\n", conn_records);
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
        fprintf(stderr, "error: cannot find %i number of records\n", conn_records);
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

        if (pid >= 0 && i == 0) {
            t1 = flb_proc_stat_create(pid);
            if (!t1) {
                fprintf(stderr, "error gathering stats for PID %i\n",
                        (int) pid);
            }
        }

        mk_list_foreach(head, connections) {
            conn = mk_list_entry(head, struct tcp_conn, _head);
            out_fd = conn->fd;

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
                round_records += records;
                round_bytes += bytes;
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
            t1 = t2;

        }
    }

    if (t1) {
        flb_proc_stat_destroy(t1);
    }

    /*
     * Create continuos snapshots until resources consumption (CPU) stabilize,
     * we assume that after two seconds without deltas in user time the process
     * finished processing our records.
     */
    if (pid >= 0) {
        int count = 0;
        int loops = 0;

        while (1) {
            t1 = flb_proc_stat_create(pid);
            sleep(1);
            t2 = flb_proc_stat_create(pid);
            flb_report_stats(r, 0, 0, t1, t2);
            loops++;

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

            /* Count total number of bytes in memory */
            total_cpu += flb_report_cpu_usage(r, t1, t2);
            total_mem += t2->r_rss;

            flb_proc_stat_destroy(t1);
            flb_proc_stat_destroy(t2);

        }
        r->sum_records = total_records;
        flb_report_summary(r);
    }

    if (r) {
        flb_report_destroy(r);
    }

    if (proc_name) {
        free(proc_name);
    }

    close(out_fd);
}

int main(int argc, char **argv)
{
    int ret;
    int opt;
    int concurrency = DEFAULT_CONCURRENCY;
    int records = DEFAULT_RECORDS;
    int seconds = DEFAULT_SECONDS;
    int increase_by = DEFAULT_INC_BY;
    int out_fd;
    int pid = -1;
    int fd_report;
    int fmt_report = FLB_REPORT_TXT;
    char *format = NULL;
    char *report = NULL;
    char *out_host = NULL;
    char *data_file = NULL;
    char *host = NULL;
    char *port = NULL;

    /* Setup long-options */
    static const struct option long_opts[] = {
        { "concurrency",   required_argument, NULL, 'c' },
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
                              "c:d:p:o:r:i:s:R:F:h", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'c':
            concurrency = atoi(optarg);
            break;
        case 'd':
            data_file = strdup(optarg);
            break;
        case 'p':
            pid = atoi(optarg);
            break;
        case 'o':
            out_host = strdup(optarg);
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

    if (!out_host) {
        host = strdup(DEFAULT_HOST);
        port = strdup(DEFAULT_PORT);
    }
    else {
        /* Parse host and port */
        char *p;
        char *sep;

        p = strchr(out_host, ':');
        if (!p) {
            host = strdup(out_host);
            port = strdup(DEFAULT_PORT);
        }
        else {
            host = strndup(out_host, p - out_host);
            *p++;
            if (!p) {
                port = strdup(DEFAULT_PORT);
            }
            else {
                port = strdup(p);
            }
        }
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

    ret = run_tcp_writer(pid, report, fmt_report, data_file,
                         host, port,
                         concurrency, records, increase_by, seconds);

    free(report);
    free(format);
    free(data_file);
    free(host);
    free(port);

    if (ret == -1) {
        exit(EXIT_FAILURE);
    }

    return 0;
}
