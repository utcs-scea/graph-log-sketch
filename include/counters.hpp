#ifndef _COUNTERS_HPP_
#define _COUNTERS_HPP_
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <asm/unistd.h>
#include <errno.h>
#include <inttypes.h>

typedef struct read_format
{
  uint64_t nr;
  struct {
      uint64_t value;
      uint64_t id;
    } values[];
} rf;

typedef struct pass_around
{
  int64_t fd0;
  uint64_t* ids;
} pa;

pa create_counters();
void reset_counters(pa pa0);
void start_counters(pa pa0);
void stop_counters(pa pa0);
void print_counters(pa pa0, std::ofstream& ofs);
uint32_t num_counters();

static char buf[4096];

static uint64_t raw_counters[] = {0x00148,0x02124,
                                  0x10000,
                                  PERF_COUNT_HW_CACHE_RESULT_ACCESS<<16|PERF_COUNT_HW_CACHE_OP_READ<<8|PERF_COUNT_HW_CACHE_L1D};

static uint64_t fix_counters[] = {PERF_COUNT_HW_INSTRUCTIONS, PERF_COUNT_HW_REF_CPU_CYCLES};
static const char* raw_strings[] = {"cycles", "instructions", "ref-cycles",
  "l2_rqsts.demand_data_rd_miss", "l2_rqsts.demand_data_rd_hit",
  "L1-dcache-load-misses", "L1-dcache-loads"};

pa create_counters()
{
  uint64_t* ids = (uint64_t*) malloc(sizeof(raw_counters) + sizeof(uint64_t)*3);
  struct perf_event_attr pea;
  uint32_t size = sizeof(struct perf_event_attr);
  memset(&pea, 0, size);
  pea.type = PERF_TYPE_HARDWARE;
  pea.size = size;
  pea.config = PERF_COUNT_HW_CPU_CYCLES;
  pea.disabled = 1;
  pea.inherit = 1;
  pea.exclude_kernel = 0;
  pea.exclude_hv = 1;
  pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
  int fd0 = syscall(__NR_perf_event_open, &pea, 0, -1, -1, 0);
  if(fd0 < 0) exit(-1);
  int fd = ioctl(fd0, PERF_EVENT_IOC_ID, &ids[0]);
  if(fd  < 0) exit(-2);
  for(uint32_t i = 0; i < sizeof(fix_counters)/sizeof(fix_counters[0]); i++)
  {
    memset(&pea, 0, size);
    pea.type = PERF_TYPE_HARDWARE;
    pea.size = size;
    pea.config = fix_counters[i];
    pea.disabled = 1;
    pea.inherit = 1;
    pea.exclude_kernel = 0;
    pea.exclude_hv = 1;
    pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
    fd = syscall(__NR_perf_event_open, &pea, 0, -1, fd0, 0);
    if(fd < 0) exit(-1);
    fd = ioctl(fd, PERF_EVENT_IOC_ID, &ids[i + 1]);
    if(fd < 0) exit(-2);
  }
  for(uint32_t i = 0; i < sizeof(raw_counters)/sizeof(raw_counters[0]); i++)
  {
    memset(&pea, 0, size);
    pea.type = PERF_TYPE_RAW;
    pea.size = size;
    pea.config = raw_counters[i];
    pea.disabled = 1;
    pea.inherit = 1;
    pea.exclude_kernel = 0;
    pea.exclude_hv = 1;
    pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
    fd = syscall(__NR_perf_event_open, &pea, 0, -1, fd0, 0);
    if(fd < 0) exit(-1);
    fd = ioctl(fd, PERF_EVENT_IOC_ID, &ids[i + 1 + sizeof(fix_counters)/sizeof(fix_counters[0])]);
    if(fd < 0) exit(-2);
  }
  pa p;
  p.fd0 = fd0;
  p.ids = ids;
  return p;
}

void reset_counters(pa pa0)
{
  int fd = ioctl(pa0.fd0, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
  if (fd < 0) exit(-10);
}

void start_counters(pa pa0)
{
  int fd = ioctl(pa0.fd0, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
  if (fd < 0) exit(-11);
}

void stop_counters(pa pa0)
{
  int fd = ioctl(pa0.fd0, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
  if (fd < 0) exit(-12);
}

void print_counters(pa pa0, std::ofstream& ofs)
{
  uint64_t* vals = (uint64_t*) malloc(sizeof(raw_counters) + sizeof(uint64_t) + sizeof(fix_counters));
  rf* rf0 = (rf*) buf;
  int i = read(pa0.fd0, buf, sizeof(buf));
  if (i < 0) exit(-3);
  for(uint32_t i = 0; i < rf0->nr; i++)
  {
    for(uint32_t j = 0; j < sizeof(raw_counters)/sizeof(raw_counters[0]) + 1 + sizeof(fix_counters); j++)
    {
      if(rf0->values[i].id == pa0.ids[j])
      {
        vals[j] = rf0->values[i].value;
        break;
      }
    }
  }
  for(uint32_t i = 0; i < rf0->nr; i++)
    ofs << vals[i] << "\t" << raw_strings[i] << std::endl;
}

uint32_t num_counters()
{
  return 1 + (sizeof(fix_counters)/sizeof(fix_counters[0])) + (sizeof(raw_counters)/sizeof(raw_counters[0]));
}


#endif
