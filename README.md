# Fluent Bit Performance Test Tools

[Fluent Bit](https://fluentbit.io) is a fast and lightweight log processor. As part of our continuous development and testing model, we are creating specific tools to test performance under different data load scenarios.

The objective of our performance tooling is to gather the following insights:

- Upon N number of records/data ingestion, measure:
  - CPU usage, user time and system time 
  - Memory usage
  - Total time required to process the data

Tests aim to run for a fixed number of time, data load can be increased per round. Every tool is able to monitor and collect resource usage metrics from the tested Fluent Bit or another logging tool as a target.

## How it Works

Every tool available is written on top of a generic framework that provides interfaces to load data files and gather metrics from a running process. 

In the following diagram, using flb-tail-writer tool as an example, it writes N amount of data to a custom log file, Fluent Bit through [Tail input plugin](https://docs.fluentbit.io/manual/input/tail) reads information from the file. Internally the Linux Kernel exposes Fluent Bit process metrics through ProcFS, where flb-tail-writer _before_ and _after_ every write/round operation gather metrics and provides insights of resources consumption.

```
+-----------------+                 +----------------+
| Proc FS (/proc) +<----------------+  Linux Kernel  |
+-----+-----+-----+                 +-----+----+-----+
      |     ^                             |    ^
      v     |                             v    |
+-----+-----+-----+                 +-----+----+-----+
|                 |                 |                |
| FLB Tail Writer +-----+     +-----+   Fluent Bit   |
|                 |     |     |     |                |
+--------+--------+     |     |     +----------------+
         |              |     |
         v              v     v
  +------+------+     +-+-----+----------+
  | Test Report |     | /var/log/out.log |
  +-------------+     +------------------+
```

As an example, consider the following test using _flb-tail-writer_ where:

- Reads samples of data from _data.log_ file
- Output data will be written to _out.log_ file
- Write 1000000 records (log lines) every second, 10 times (called as 10 seconds).
- Monitor resources usage of Fluent Bit process ID (PID).
- Stop Monitoring Fluent Bit process once the process becomes almost idle for 3 seconds.

```bash
$ bin/flb-tail-writer -d data.log -r 100000 -s 10 -o out.log -p `pidof fluent-bit`

records  write (bytes)     write |  % cpu  user t (ms)  system t (ms)  Mem (bytes)    Mem
-------- -------------  -------- + ------  -----------  -------------  -----------  -----
 100000       13210635    12.60M |   3.00           30              0      6234112  5.95M
 100000       13210635    12.60M |   6.00           30             30      6086656  5.80M
 100000       13210635    12.60M |   5.00           40             10      6111232  5.83M
 100000       13210635    12.60M |   5.00           30             20      6205440  5.92M
 100000       13210635    12.60M |   6.00           30             30      6205440  5.92M
 100000       13210635    12.60M |   5.00           40             10      6262784  5.97M
 100000       13210635    12.60M |   6.00           40             20      6262784  5.97M
 100000       13210635    12.60M |   6.00           30             30      6262784  5.97M
 100000       13210635    12.60M |   4.00           20             20      6262784  5.97M
 100000       13210635    12.60M |   5.00           40             10      6262784  5.97M
      0              0       0 b |   0.00            0              0      6262784  5.97M
      0              0       0 b |   0.00            0              0      6262784  5.97M
      0              0       0 b |   0.00            0              0      6262784  5.97M
      
- Summary
  - Process     : fluent-bit
  - PID         : 28781
  - Elapsed time: 14 seconds
  - Avg Memory  : 8.35M
  - Avg CPU     : 11%
```



## Tools Available

| Tool        |                     Fluent Bit Target                     | Description                                  |
| ----------- | :-------------------------------------------------------: | -------------------------------------------- |
| Tail Writer | [Tail input](https://docs.fluentbit.io/manual/input/tail) | Writes large amount of data into a log file. |

## Build Instructions

### Requirements

- C compiler
- CMake3
- Linux environment

### Build

Run the following command to compile the tools:

```bash
$ cd build/
$ make
```

## Comments about Performance and Benchmarking

Performance is always critical and when managing data at high scale there are many corners where is possible to improve and make it better. 

When measuring performance is important to understand the variables that can affect a running _monitored_ service. If you are comparing same tool like Fluent Bit v/s Fluent Bit is not a hard task, but if you aim to compare Fluent Bit against other solution in the same space, you have to do an extra work and make sure that the setup and conditions are the same, e.g: make sure buffer sizes are the same on both tools. 

>  Running a performance test using default options in different services will lead to unreliable results. 

Story: some years ago I was working in one of my [HTTP servers](http://monkey-project.com) projects. We got into a benchmark virtual battle against a proprietary web server. They claimed aims to be faster that all open source options available (e.g: Nginx, Lighttpd, Apache, etc)... and benchmark results shows that their project was _outstanding_ leaving every other option behind. 

After digging a bit more and starting measuring what was doing that web server from a operating system level, we ended up discovering that it was _caching_ every HTTP request and response, so if it get one million request for the same end-point, it sends the same response over and over, without extra processing. Basically it was _prepared_ before hand to cheat if it was benchmarked. 

> Upon sending a HTTP request with an URI that changed the query string variable (e.g: /?a=1..) every time, it was slow as hell :)

On that moment I learn how important was to measure every aspect of a running service. That's why the simple metrics of CPU time in user/kernel space and memory usage are really important. 

final tip: if you are the user, try to do your own benchmarks for your own conditions and scenario. Trust in our performance tooling but don't trust in benchmarks reports made by us (maintainers) or vendors XD .

## License

This program is under the terms of the [Apache License v2.0](http://www.apache.org/licenses/LICENSE-2.0).

## Authors

- [Eduardo Silva ](https://twitter.com/edsiper)