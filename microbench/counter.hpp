// counter.hpp
#ifndef COUNTER_HPP
#define COUNTER_HPP

#include <linux/perf_event.h>
#include <unistd.h>
#include <syscall.h>
#include <sys/ioctl.h>
#include <string>
#include <iostream>
#include <cstring>
#include <cerrno>
#include <vector>

class PerfEvent {
private:
    int fd;
    long long count;

    static long perf_event_open(struct perf_event_attr* hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags) {
        return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
    }

public:
    PerfEvent() : fd(-1), count(0) {}

    void setup(long long type, long long config) {
        struct perf_event_attr pe;
        memset(&pe, 0, sizeof(struct perf_event_attr));
        pe.type = type;
        pe.size = sizeof(struct perf_event_attr);
        pe.config = config;
        pe.disabled = 1;
        pe.exclude_kernel = 1;
        pe.exclude_hv = 1;

        fd = perf_event_open(&pe, 0, -1, -1, 0);
        if (fd == -1) {
            std::cerr << "Error opening perf event: " << std::strerror(errno) << std::endl;
        }
    }

    void start() {
        if (fd != -1) {
            ioctl(fd, PERF_EVENT_IOC_RESET, 0);
            ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
        }
    }

    void stop() {
        if (fd != -1) {
            ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
            read(fd, &count, sizeof(long long));
        }
    }

    long long getCount() const {
        return count;
    }

    ~PerfEvent() {
        if (fd != -1) {
            close(fd);
        }
    }
};

class PerfScopeBenchmarker {
private:
    std::vector<PerfEvent> events;
    std::string scopeName;

public:
    PerfScopeBenchmarker(const std::string& name) : scopeName(name), events(4) {
        events[0].setup(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES);
        events[1].setup(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES);
        events[2].setup(PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
        events[3].setup(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);

        for (auto& event : events) {
            event.start();
        }
    }

    ~PerfScopeBenchmarker() {
        for (auto& event : events) {
            event.stop();
        }

        std::cout << "Benchmark results for " << scopeName << ":\n";
        std::cout << "Cache Misses: " << events[0].getCount() << "\n";
        std::cout << "Cache References: " << events[1].getCount() << "\n";
        std::cout << "Instructions: " << events[2].getCount() << "\n";
        std::cout << "CPU Cycles: " << events[3].getCount() << "\n";
    }
};

#define BENCHMARK_SCOPE(name) PerfScopeBenchmarker benchmarker##__LINE__(name)

#endif // COUNTER_HPP
