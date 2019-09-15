/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Fluent Bit
 *  ==========
 *  Copyright (C) 2019       The Fluent Bit Authors
 *  Copyright (C) 2014-2019  Eduardo Silva <edsiper@gmail.com>
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

/*
 * FYI: I wrote this code back in 2014, it's being modified for this project
 * purposes, original code is located here:
 *
 * https://github.com/edsiper/wr
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#include "flb_proc.h"

/*
 * Read file content to a memory buffer,
 * Use this function just for really SMALL files
 */
static char *file_to_buffer(const char *path)
{
    FILE *fp;
    char *buffer;
    long bytes;

    if (access(path, R_OK) != 0) {
        perror("access");
        fprintf(stderr, "error: cannot open '%s'\n", path);
        return NULL;
    }

    if (!(fp = fopen(path, "r"))) {
        perror("fopen");
        fprintf(stderr, "error: cannot fopen '%s'\n", path);
        return NULL;
    }

    buffer = malloc(PROC_STAT_BUF_SIZE);
    if (!buffer) {
        perror("malloc");
        fprintf(stderr, "error: could not allocate memory to open '%s'\n",
                path);
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    bytes = fread(buffer, PROC_STAT_BUF_SIZE, 1, fp);
    if (bytes < 0) {
        free(buffer);
        fclose(fp);
        printf("Error: could not read '%s'\n", path);
        exit(EXIT_FAILURE);
    }

    fclose(fp);
    return buffer;
}

struct flb_proc_task *flb_proc_stat_create(pid_t pid)
{
    int ret;
    int fields;
    int cpu_hz = sysconf(_SC_CLK_TCK);
    char *p, *q;
    char *buf;
    char pid_path[PROC_PID_SIZE];
    char tmp[256];
    struct flb_proc_task *t;

    t = calloc(1, sizeof(struct flb_proc_task));
    if (!t) {
      perror("calloc");
      return NULL;
    }

    /* Compose path for /proc/PID/stat */
    ret = snprintf(pid_path, PROC_PID_SIZE, "/proc/%i/stat", pid);
    if (ret < 0) {
        fprintf(stderr, "error: could not compose PID path: %s\n", pid_path);
        free(t);
        return NULL;
    }

    buf = file_to_buffer(pid_path);
    if (!buf) {
        fprintf(stderr, "error: could not read stat file data: %s\n", pid_path);
        free(t);
        return NULL;
    }

    p = buf;
    while (*p != ')') *p++;
    *p++;

    /* Read pending values */
    char *x = "%*d %s %*c %*d %*d %*d %*d %*d %*lu %*lu %*lu %*lu %*lu %lu %lu %*ld %*ld %*ld %*ld %*ld %*ld %*lu %*lu %ld ";

    fields = sscanf(buf, x,
                    &tmp,
                    &t->utime,
                    &t->stime,
                    &t->rss);

    p = strndup(tmp + 1, strlen(tmp + 1) - 1);
    memcpy(t->name, p, strlen(p));

    /* Internal conversion */
    t->r_rss      = (t->rss * getpagesize());
    t->r_utime_s  = (t->utime / cpu_hz);
    t->r_utime_ms = ((t->utime * 1000) / cpu_hz);
    t->r_stime_s  = (t->stime / cpu_hz);
    t->r_stime_ms = ((t->stime * 1000) / cpu_hz);

    /* Set timestamp */
    clock_gettime(CLOCK_REALTIME, &t->ts);

    free(buf);
    return t;
}

void flb_proc_stat_destroy(struct flb_proc_task *t)
{
    free(t);
}

void flb_proc_stat_print(struct flb_proc_task *t)
{
    printf("utime       = %lu\n", t->utime);
    printf("stime       = %lu\n", t->stime);
    printf("rss         = %ld\n", t->rss);
    fflush(stdout);
}
