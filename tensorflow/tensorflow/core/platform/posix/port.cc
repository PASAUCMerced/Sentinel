/* Copyright 2015 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "absl/base/internal/sysinfo.h"

#include "tensorflow/core/platform/cpu_info.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/mem.h"
#include "tensorflow/core/platform/numa.h"
#include "tensorflow/core/platform/snappy.h"
#include "tensorflow/core/platform/types.h"
#include "tensorflow/core/platform/posix/port.h"

#if defined(__linux__) && !defined(__ANDROID__)
#include <sched.h>
#include <sys/sysinfo.h>
#else
#include <sys/syscall.h>
#endif

#if (__x86_64__ || __i386__)
#include <cpuid.h>
#endif

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <map>
#ifdef TF_USE_SNAPPY
#include "snappy.h"
#endif
#if (defined(__APPLE__) && defined(__MACH__)) || defined(__FreeBSD__) || \
    defined(__HAIKU__)
#include <thread>
#endif

#define KB(x)   ((size_t) (x) << 10)
#define MB(x)   ((size_t) (x) << 20)
//const size_t pagesize = sysconf(_SC_PAGESIZE);
//const size_t linesize = sizeof(void*);
//using namespace std;

namespace tensorflow {
namespace port {

MyMemoryAllocator MMA;

void InitMain(const char* usage, int* argc, char*** argv) {
//	MMA.SetValue(1);
}

string Hostname() {
  char hostname[1024];
  gethostname(hostname, sizeof hostname);
  hostname[sizeof hostname - 1] = 0;
  return string(hostname);
}

int NumSchedulableCPUs() {
#if defined(__linux__) && !defined(__ANDROID__)
  cpu_set_t cpuset;
  if (sched_getaffinity(0, sizeof(cpu_set_t), &cpuset) == 0) {
    return CPU_COUNT(&cpuset);
  }
  perror("sched_getaffinity");
#endif
#if (defined(__APPLE__) && defined(__MACH__)) || defined(__FreeBSD__) || \
    defined(__HAIKU__)
  unsigned int count = std::thread::hardware_concurrency();
  if (count > 0) return static_cast<int>(count);
#endif
  const int kDefaultCores = 4;  // Semi-conservative guess
  fprintf(stderr, "can't determine number of CPU cores: assuming %d\n",
          kDefaultCores);
  return kDefaultCores;
}

int MaxParallelism(int numa_node) {
  if (numa_node != port::kNUMANoAffinity) {
    // Assume that CPUs are equally distributed over available NUMA nodes.
    // This may not be true, but there isn't currently a better way of
    // determining the number of CPUs specific to the requested node.
    return NumSchedulableCPUs() / port::NUMANumNodes();
  }
  return NumSchedulableCPUs();
}

int NumTotalCPUs() {
  int count = absl::base_internal::NumCPUs();
  return (count <= 0) ? kUnknownCPU : count;
}

int GetCurrentCPU() {
#if defined(__linux__) && !defined(__ANDROID__)
  return sched_getcpu();
  // Attempt to use cpuid on all other platforms.  If that fails, perform a
  // syscall.
#elif defined(__cpuid) && !defined(__APPLE__)
  // TODO(b/120919972): __cpuid returns invalid APIC ids on OS X.
  uint32_t eax = 0;
  uint32_t ebx = 0;
  uint32_t ecx = 0;
  uint32_t edx = 0;
  __cpuid(/*level=*/1, eax, ebx, ecx, edx);
  if ((edx & /*bit_APIC=*/(1 << 9)) != 0) {
    // EBX bits 24-31 are APIC ID
    return (ebx & 0xFF) >> 24;
  }
#elif defined(__NR_getcpu)
  unsigned int cpu;
  if (syscall(__NR_getcpu, &cpu, NULL, NULL) < 0) {
    return kUnknownCPU;
  } else {
    return static_cast<int>(cpu);
  }
#endif
  return kUnknownCPU;
}

int NumHyperthreadsPerCore() {
  static const int ht_per_core = tensorflow::port::CPUIDNumSMT();
  return (ht_per_core > 0) ? ht_per_core : 1;
}

bool NUMAEnabled() {
  // Not yet implemented: coming soon.
  return false;
}

int NUMANumNodes() { return 1; }

void NUMASetThreadNodeAffinity(int node) {}

int NUMAGetThreadNodeAffinity() { return kNUMANoAffinity; }

void* AlignedMalloc(size_t size, int minimum_alignment) {
  //jie:
  //if( minimum_alignment > 64)
  //  printf("size = %d, minimum_alignment = %u\n", size, minimum_alignment);
#if defined(__ANDROID__)
  return memalign(minimum_alignment, size);
#else  // !defined(__ANDROID__)
  //void* ptr = nullptr;
  // posix_memalign requires that the requested alignment be at least
  // sizeof(void*). In this case, fall back on malloc which should return
  // memory aligned to at least the size of a pointer.
  //const int required_alignment = sizeof(void*);
  //if (minimum_alignment < required_alignment) return Malloc(size);
  /*{
    return  MMA.SmallMalloc();
  }*/
  //  return MMA.MMAMalloc(size);
  //int real_allocate = (minimum_alignment+10)/pagesize;
  /*if (size/pagesize >= 1)
  {
	//printf("use one page\n");
	ptr = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if( ptr != nullptr)
	{
		MMA.SetValue(ptr, size);
		return ptr;
	}
}*/
  /*int err = posix_memalign(&ptr, minimum_alignment, size);
  if (err != 0) {
    return nullptr;
  } else {
    return ptr;
  }*/
  return MMA.MyAllocate(size);

#endif
}

void AlignedFree(void* aligned_memory)// { Free(aligned_memory); }
{MMA.MyFree(aligned_memory);}

void* Malloc(size_t size) { return malloc(size); }
//{
//  return MMA.MyAllocate(size);
//}
void* Realloc(void* ptr, size_t size) { return realloc(ptr, size); }

void Free(void* ptr) {
//	MMA.MyFree(ptr);}
//if(ptr==NULL)
//  return;
//printf("Call Free\n");
free(ptr); }

void* NUMAMalloc(int node, size_t size, int minimum_alignment) {
  return AlignedMalloc(size, minimum_alignment);
}

void NUMAFree(void* ptr, size_t size) { Free(ptr); }

int NUMAGetMemAffinity(const void* addr) { return kNUMANoAffinity; }

void MallocExtension_ReleaseToSystem(std::size_t num_bytes) {
  // No-op.
}

std::size_t MallocExtension_GetAllocatedSize(const void* p) { return 0; }

void AdjustFilenameForLogging(string* filename) {
  // Nothing to do
}

bool Snappy_Compress(const char* input, size_t length, string* output) {
#ifdef TF_USE_SNAPPY
  output->resize(snappy::MaxCompressedLength(length));
  size_t outlen;
  snappy::RawCompress(input, length, &(*output)[0], &outlen);
  output->resize(outlen);
  return true;
#else
  return false;
#endif
}

bool Snappy_GetUncompressedLength(const char* input, size_t length,
                                  size_t* result) {
#ifdef TF_USE_SNAPPY
  return snappy::GetUncompressedLength(input, length, result);
#else
  return false;
#endif
}

bool Snappy_Uncompress(const char* input, size_t length, char* output) {
#ifdef TF_USE_SNAPPY
  return snappy::RawUncompress(input, length, output);
#else
  return false;
#endif
}

string Demangle(const char* mangled) { return mangled; }

double NominalCPUFrequency() {
  return absl::base_internal::NominalCPUFrequency();
}

int64 AvailableRam() {
#if defined(__linux__) && !defined(__ANDROID__)
  struct sysinfo info;
  int err = sysinfo(&info);
  if (err == 0) {
    return info.freeram;
  }
#endif
  return INT64_MAX;
}

}  // namespace port
}  // namespace tensorflow
