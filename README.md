# Fluent Bit Performance Test Tools

[Fluent Bit](https://fluentbit.io) is a fast and lightweight log processor. As part of our continuous development and testing model, we provide specific tools to test performance under different data load scenarios.

The objective of our performance tooling is to gather the following insights:

- Upon N number of records/data ingestion, measure:
  - CPU usage, user time and system time
  - Memory usage
  - Total time required to process the data

Tests aim to run for a fixed number of time, data load can be increased per round. Every tool is able to monitor and collect resource usage metrics from the tested Fluent Bit or another logging tool as a target.

## How it Works

Every tool available is written on top of a generic framework that provides interfaces to load data files, gather metrics nad generate a report from a running process.

In the following diagram, using ```flb-tail-writer``` tool as an example, it writes N amount of records (lines) to a custom log file, Fluent Bit through [Tail input plugin](https://docs.fluentbit.io/manual/input/tail) reads information from the file. Internally the Linux Kernel exposes Fluent Bit process metrics through ProcFS, where flb-tail-writer _before_ and _after_ every write/round operation gather metrics and provides insights of resources consumption.

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

As an example, consider the following test using ```flb-tail-writer``` where:

- Reads samples of data from _data.log_ file
- Output data will be written to _out.log_ file
- Write 1000000 records (log lines) every second, 10 times (called as 10 seconds).
- Monitor resources usage of Fluent Bit process ID (PID).
- Stop Monitoring Fluent Bit process once the process becomes almost idle for 3 seconds.

```bash
 records   write (b)     write   secs |  % cpu  user (ms)  sys (ms)  Mem (bytes)      Mem
--------  ----------  --------  ----- + ------  ---------  --------  -----------  -------
 1000000   131881447   125.77M   1.09 |  58.04        570        60     38785024   36.99M
 1000000   131881447   125.77M   1.08 |  61.22        590        70     61644800   58.79M
 1000000   131881447   125.77M   1.07 |  57.70        560        60     88682496   84.57M
 1000000   131881447   125.77M   1.10 |  57.97        590        50    115126272  109.79M
 1000000   131881447   125.77M   1.10 |  62.70        630        60    146522112  139.73M
 1000000   131881447   125.77M   1.09 |  67.85        710        30    155217920  148.03M
 1000000   131881447   125.77M   1.07 |  66.07        640        70      5005312    4.77M
 1000000   131881447   125.77M   1.08 |  61.83        600        70      4677632    4.46M
 1000000   131881447   125.77M   1.08 |  60.42        630        20      4677632    4.46M
 1000000   131881447   125.77M   1.08 |  65.09        610        90      4677632    4.46M
       0           0       0 b   1.00 |   0.00          0         0      4677632    4.46M
       0           0       0 b   1.00 |   0.00          0         0      4677632    4.46M
       0           0       0 b   1.00 |   0.00          0         0      4677632    4.46M

- Summary
  - Process     : fluent-bit
  - PID         : 27207
  - Elapsed Time: 10.84 seconds
  - Avg Memory  : 46.88M
  - Avg CPU     : 61.89%
```

## Report Details

The report have two panes, left side belongs to the information provided by the running tools in terms
of data ingestion and the right side the metrics collected from the monitored process.

| column    | description                                                |
| --------- | ---------------------------------------------------------- |
| records   | Number of records ingested in that specific round.         |
| write (b) | Total size of the records ingested represented in _bytes_. |
| write     | Human readable version of written bytes.                   |
| secs      | Elapsed time on writing the data.                          |
| % cpu     | Average CPU usage for the monitored process.               |

## Tools Available

| Tool        |                     Fluent Bit Target                     | Description                                  |
| ----------- | :-------------------------------------------------------: | -------------------------------------------- |
| Tail Writer | [Tail input](https://docs.fluentbit.io/manual/input/tail) | Writes large amount of data into a log file. |
| TCP Writer  | [TCP input](https://docs.fluentbit.io/manual/input/tail), [Syslog input](https://docs.fluentbit.io/manual/input/syslog) (tcp mode) | Writes large amount of data over a TCP socket. |

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
