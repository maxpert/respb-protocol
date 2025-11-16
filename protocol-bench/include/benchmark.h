/*
 * Benchmark Utilities and Definitions
 * Performance measurement framework
 */

#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

// Maximum latency samples to collect
#define MAX_LATENCY_SAMPLES 10000

// Workload types
typedef enum {
    WORKLOAD_FILE = 0,
    WORKLOAD_SMALL_KEYS,
    WORKLOAD_MEDIUM_KEYS,
    WORKLOAD_LARGE_VALUES,
    WORKLOAD_MIXED
} workload_type_t;

// Workload structure
typedef struct {
    uint8_t *data;
    size_t size;
    size_t current_pos;
} workload_t;

// Performance metrics
typedef struct {
    uint64_t commands_processed;
    uint64_t bytes_processed;
    uint64_t total_time_ns;
    uint64_t cpu_time_us;
    uint64_t peak_memory_kb;
    
    // Latency statistics
    uint64_t latency_samples[MAX_LATENCY_SAMPLES];
    size_t latency_sample_count;
    uint64_t total_latency_ns;
    uint64_t min_latency_ns;
    uint64_t max_latency_ns;
    uint64_t avg_latency_ns;
    uint64_t p50_latency_ns;
    uint64_t p90_latency_ns;
    uint64_t p99_latency_ns;
} benchmark_metrics_t;

// Timer for benchmarking
typedef struct {
    uint64_t start_ns;
    uint64_t start_utime;
    uint64_t start_stime;
} benchmark_timer_t;

// Benchmark configuration
typedef struct {
    int iterations;
    int sample_latency;
    int bench_resp;
    int bench_respb;
    workload_type_t workload_type;
    const char *resp_workload_file;
    const char *respb_workload_file;
    benchmark_metrics_t resp_metrics;
    benchmark_metrics_t respb_metrics;
} benchmark_config_t;

// Workload functions
workload_t *workload_load(const char *filename);
workload_t *workload_generate_synthetic(size_t target_size, workload_type_t type);
void workload_free(workload_t *wl);
void workload_reset(workload_t *wl);
int workload_has_more(const workload_t *wl);
size_t workload_remaining(const workload_t *wl);
int workload_save(const workload_t *wl, const char *filename);

// Metrics functions
void benchmark_metrics_init(benchmark_metrics_t *metrics);
void benchmark_timer_start(benchmark_timer_t *timer);
uint64_t benchmark_timer_elapsed_ns(const benchmark_timer_t *timer);
void benchmark_timer_stop(benchmark_timer_t *timer, benchmark_metrics_t *metrics);
void benchmark_record_latency(benchmark_metrics_t *metrics, uint64_t latency_ns);
void benchmark_compute_percentiles(benchmark_metrics_t *metrics);
void benchmark_print_metrics(const benchmark_metrics_t *metrics, const char *protocol_name);
void benchmark_print_comparison(const benchmark_metrics_t *resp_metrics,
                               const benchmark_metrics_t *respb_metrics);

// Benchmark runner
int run_benchmark(benchmark_config_t *config);
void print_usage(const char *prog_name);

#endif // BENCHMARK_H
