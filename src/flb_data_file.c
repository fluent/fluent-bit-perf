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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

static void *mmap_file(int fd, size_t *file_size)
{
    int ret;
    int oflags = 0;
    size_t size = 0;
    void *map;
    struct stat fst;

    /* Validate file descriptor */
    ret = fstat(fd, &fst);
    if (ret == -1) {
        fprintf(stderr, "error: loading mmap file, file descriptor = '%i'\n",
                fd);
        return NULL;
    }
    size = fst.st_size;

    if (size == 0) {
        fprintf(stderr, "error: data file size is zero\n");
        return NULL;
    }

    /* Mmap flags */
    oflags = PROT_READ;

    map = mmap(0, size, oflags, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        fprintf(stderr, "error: mapping file, file descriptor = '%i'\n", fd);
        return NULL;
    }

    *file_size = size;

    return map;
}

int flb_data_file_load(char *path, char **out_buf, size_t *out_size)
{
    int fd;
    char *buf;
    size_t size;

    fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("open");
        fprintf(stderr, "error: cannot open data file '%s'\n", path);
        return -1;
    }

    buf = mmap_file(fd, &size);
    if (!buf) {
        close(fd);
        return -1;
    }

    *out_buf = buf;
    *out_size = size;

    return fd;
}

void flb_data_file_unload(void *map, size_t size)
{
    munmap(map, size);
}

/*
 * Given a buffer and it size, retrieve the offset position that have N number
 * of records (lines).
 */
int flb_data_file_offset_records(int n_records, char *buf,
                                 size_t size, off_t *offset)
{
    int total = 0;
    char *p;
    char *start;
    char *end;

    p = buf;
    start = buf;
    end = buf + size;
    while (start < end) {
        p = strchr(start, '\n');
        if (p) {
            total++;
        }
        else {
            break;
        }

        if (total == n_records) {
            break;
        }

        start = p + 1;
    }

    if (total < n_records) {
        return -1;
    }

    *offset = (p - buf) + 1;
    return 0;
}
