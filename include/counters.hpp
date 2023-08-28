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
#include <fstream>

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

struct counter
{
  uint32_t type;
  uint64_t config;
  const char raw_string[7];
};

#define PCHCRM PERF_COUNT_HW_CACHE_RESULT_MISS
#define PCHCRA PERF_COUNT_HW_CACHE_RESULT_ACCESS
#define PCHCOR PERF_COUNT_HW_CACHE_OP_READ
#define PCHW1D PERF_COUNT_HW_CACHE_L1D

static constexpr counter counts[] =
#ifdef BROADWELL
{ { .type = PERF_TYPE_HARDWARE, .config = PERF_COUNT_HW_CPU_CYCLES    , .raw_string = "cycles"},
  { .type = PERF_TYPE_HARDWARE, .config = PERF_COUNT_HW_INSTRUCTIONS  , .raw_string = "instru"},
  { .type = PERF_TYPE_HARDWARE, .config = PERF_COUNT_HW_REF_CPU_CYCLES, .raw_string = "ref_cy"},
  { .type = PERF_TYPE_HW_CACHE, .config = PCHCRM<<16|PCHCOR<<8|PCHW1D , .raw_string = "l1_dlm"},
  { .type = PERF_TYPE_HW_CACHE, .config = PCHCRA<<16|PCHCOR<<8|PCHW1D , .raw_string = "l1_dla"},
  { .type = PERF_TYPE_RAW     , .config = 0x00148                     , .raw_string = "l2rd_m"},
  { .type = PERF_TYPE_RAW     , .config = 0x02124                     , .raw_string = "l2rd_h"},
};
#elseif SKYLAKE
{ { .type = PERF_TYPE_HARDWARE, .config = PERF_COUNT_HW_CPU_CYCLES    , .raw_string = "cycles"},
  { .type = PERF_TYPE_HARDWARE, .config = PERF_COUNT_HW_INSTRUCTIONS  , .raw_string = "instru"},
  { .type = PERF_TYPE_HARDWARE, .config = PERF_COUNT_HW_REF_CPU_CYCLES, .raw_string = "ref_cy"},
  { .type = PERF_TYPE_HW_CACHE, .config = PCHCRM<<16|PCHCOR<<8|PCHW1D , .raw_string = "l1_dlm"},
  { .type = PERF_TYPE_HW_CACHE, .config = PCHCRA<<16|PCHCOR<<8|PCHW1D , .raw_string = "l1_dla"},
  { .type = PERF_TYPE_RAW     , .config = 0x02124                     , .raw_string = "l2rd_m"},
  { .type = PERF_TYPE_RAW     , .config = 0x0C124                     , .raw_string = "l2rd_h"},
};
#else
{ { .type = PERF_TYPE_HARDWARE, .config = PERF_COUNT_HW_CPU_CYCLES    , .raw_string = "cycles"},
  { .type = PERF_TYPE_HARDWARE, .config = PERF_COUNT_HW_INSTRUCTIONS  , .raw_string = "instru"},
  { .type = PERF_TYPE_HW_CACHE, .config = PCHCRM<<16|PCHCOR<<8|PCHW1D , .raw_string = "l1_dlm"},
  { .type = PERF_TYPE_HW_CACHE, .config = PCHCRA<<16|PCHCOR<<8|PCHW1D , .raw_string = "l1_dla"},
};
#endif

#ifdef BENCH
pa create_counters()
{
  uint64_t* ids = (uint64_t*) malloc(sizeof(counts)/sizeof(counts[0])* sizeof(uint64_t));
  struct perf_event_attr pea;
  uint32_t size = sizeof(struct perf_event_attr);
  memset(&pea, 0, size);
  pea.type = counts[0].type;
  pea.size = size;
  pea.config = counts[0].config;
  pea.disabled = 1;
  pea.inherit = 1;
  pea.exclude_kernel = 0;
  pea.exclude_hv = 1;
  pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
  int fd0 = syscall(__NR_perf_event_open, &pea, 0, -1, -1, 0);
  if(fd0 < 0) exit(-1);
  int fd = ioctl(fd0, PERF_EVENT_IOC_ID, &ids[0]);
  if(fd  < 0) exit(-2);
  for(uint32_t i = 1; i < sizeof(counts)/sizeof(counts[0]); i++)
  {
    memset(&pea, 0, size);
    pea.type = counts[i].type;
    pea.size = size;
    pea.config = counts[i].config;
    pea.disabled = 1;
    pea.inherit = 1;
    pea.exclude_kernel = 0;
    pea.exclude_hv = 1;
    pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
    fd = syscall(__NR_perf_event_open, &pea, 0, -1, fd0, 0);
    if(fd < 0) exit(-1);
    fd = ioctl(fd, PERF_EVENT_IOC_ID, &ids[i]);
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
  uint64_t* vals = (uint64_t*) malloc(sizeof(counts)/sizeof(counts[0]) * sizeof(uint64_t));
  rf* rf0 = (rf*) buf;
  int i = read(pa0.fd0, buf, sizeof(buf));
  if (i < 0) exit(-3);
  for(uint32_t i = 0; i < rf0->nr; i++)
  {
    for(uint32_t j = 0; j < sizeof(counts)/sizeof(counts[0]); j++)
    {
      if(rf0->values[i].id == pa0.ids[j])
      {
        vals[j] = rf0->values[i].value;
        break;
      }
    }
  }
  for(uint32_t i = 0; i < rf0->nr; i++)
    ofs << vals[i] << "\t" << counts[i].raw_string << std::endl;
  free(vals);
}

uint32_t num_counters()
{
  return sizeof(counts)/sizeof(counts[0]);
}
#else
pa create_counters()
{
  pa p;
  p.fd0 = -1;
  p.ids = nullptr;
  return p;
}

void reset_counters(pa pa0) {}
void start_counters(pa pa0) {}
void stop_counters(pa pa0) {}
void print_counters(pa pa0, std::ofstream& ofs) { (void) buf;}
uint32_t num_counters() { return 0; }
#endif


#endif
