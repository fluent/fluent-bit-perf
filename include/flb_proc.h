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

#ifndef FLB_PROC_H
#define FLB_PROC_H

#define PROC_PID_SIZE      1024
#define PROC_STAT_BUF_SIZE 1024

/* Our tast struct to read the /proc/PID/stat values */
struct flb_proc_task {
    char name[256];
    unsigned long utime;	  /* %lu */
    unsigned long stime; 	  /* %lu */
    long rss;		  	      /* %ld */

    /* Internal resource conversion */
    long  r_rss;                /* bytes = (rss * PAGESIZE)        */
    unsigned long r_utime_s;    /* seconds = (utime / _SC_CLK_TCK) */
    unsigned long r_utime_ms;   /* milliseconds = ((utime * 1000) / CPU_HZ) */
    unsigned long r_stime_s;    /* seconds = (utime / _SC_CLK_TCK)          */
    unsigned long r_stime_ms;   /* milliseconds = ((utime * 1000) / CPU_HZ) */
};

struct flb_proc_task *flb_proc_stat_create(pid_t pid);
void flb_proc_stat_destroy(struct flb_proc_task *t);
void flb_proc_stat_print(struct flb_proc_task *t);

#endif
