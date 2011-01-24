#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "cmph_benchmark.h"

typedef struct {
  const char* name;
  void (*func)(int);
  struct rusage begin;
  struct rusage end;
} benchmark_t;

static benchmark_t* global_benchmarks = NULL;

/* Subtract the `struct timeval' values X and Y,
   storing the result in RESULT.
   Return 1 if the difference is negative, otherwise 0.  */

int timeval_subtract ( 
    struct timeval *result, struct timeval *x, struct timeval* y) {
  /* Perform the carry for the later subtraction by updating y. */
  if (x->tv_usec < y->tv_usec) {
    int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
    y->tv_usec -= 1000000 * nsec;
    y->tv_sec += nsec;
  }
  if (x->tv_usec - y->tv_usec > 1000000) {
    int nsec = (x->tv_usec - y->tv_usec) / 1000000;
    y->tv_usec += 1000000 * nsec;
    y->tv_sec -= nsec;
  }

  /* Compute the time remaining to wait.
     tv_usec is certainly positive. */
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_usec = x->tv_usec - y->tv_usec;

  /* Return 1 if result is negative. */
  return x->tv_sec < y->tv_sec;
}

benchmark_t* find_benchmark(const char* name) {
  benchmark_t* benchmark = global_benchmarks;
  while (benchmark->name != NULL) if (strcmp(benchmark->name, name) != 0) break;
  if (!benchmark->name) return NULL;
  return benchmark;
}

int global_benchmarks_length() {
  benchmark_t* benchmark;
  int length = 0;
  if (global_benchmarks == 0) return 0;
  benchmark = global_benchmarks;
  while (benchmark->name != NULL) ++length;
  return length;
}

void bm_register(const char* name, void (*func)(int), int iters) {
  benchmark_t benchmark;
  int length = global_benchmarks_length();
  benchmark.name = name;
  benchmark.func = func;
  assert(!find_benchmark(name));
  global_benchmarks = realloc(global_benchmarks, length + 1);
  global_benchmarks[length] = benchmark;
}

void bm_start(const char* name) {
  benchmark_t* benchmark;
  struct rusage rs;

  benchmark = find_benchmark(name);
  int ret = getrusage(RUSAGE_SELF, &rs);  
  if (ret != 0) {
    perror("rusage failed");    
    exit(-1);
  }
  benchmark->begin = rs;
  (*benchmark->func)(1);
}

void bm_end(const char* name) { 
  benchmark_t* benchmark;
  struct rusage rs;

  int ret = getrusage(RUSAGE_SELF, &rs);  
  if (ret != 0) {
    perror("rusage failed");    
    exit(-1);
  }

  benchmark = find_benchmark(name);
  benchmark->end = rs;

  struct timeval utime;
  timeval_subtract(&utime, &benchmark->end.ru_utime, &benchmark->begin.ru_utime);
  struct timeval stime;
  timeval_subtract(&stime, &benchmark->end.ru_stime, &benchmark->begin.ru_stime);
  
  printf("User cpu time used: %ld.%6ld\n", utime.tv_sec, utime.tv_usec);
  printf("System cpu time used: %ld.%6ld\n", stime.tv_sec, stime.tv_usec);
}
