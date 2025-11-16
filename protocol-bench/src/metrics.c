/*
 * Metrics Collection and Reporting
 */

#include "benchmark.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

void benchmark_metrics_init(benchmark_metrics_t *metrics) {
    memset(metrics, 0, sizeof(benchmark_metrics_t));
    metrics->min_latency_ns = UINT64_MAX;
}

void benchmark_timer_start(benchmark_timer_t *timer) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    timer->start_ns = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    timer->start_utime = (uint64_t)usage.ru_utime.tv_sec * 1000000ULL + usage.ru_utime.tv_usec;
    timer->start_stime = (uint64_t)usage.ru_stime.tv_sec * 1000000ULL + usage.ru_stime.tv_usec;
}

uint64_t benchmark_timer_elapsed_ns(const benchmark_timer_t *timer) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now_ns = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    return now_ns - timer->start_ns;
}

void benchmark_timer_stop(benchmark_timer_t *timer, benchmark_metrics_t *metrics) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t end_ns = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    uint64_t end_utime = (uint64_t)usage.ru_utime.tv_sec * 1000000ULL + usage.ru_utime.tv_usec;
    uint64_t end_stime = (uint64_t)usage.ru_stime.tv_sec * 1000000ULL + usage.ru_stime.tv_usec;
    
    metrics->total_time_ns = end_ns - timer->start_ns;
    metrics->cpu_time_us = (end_utime - timer->start_utime) + (end_stime - timer->start_stime);
    
    // Memory stats (Linux-specific, would need platform abstraction)
    metrics->peak_memory_kb = usage.ru_maxrss;
#ifdef __APPLE__
    // macOS reports in bytes, convert to KB
    metrics->peak_memory_kb /= 1024;
#endif
}

void benchmark_record_latency(benchmark_metrics_t *metrics, uint64_t latency_ns) {
    if (metrics->latency_sample_count < MAX_LATENCY_SAMPLES) {
        metrics->latency_samples[metrics->latency_sample_count++] = latency_ns;
    }
    
    metrics->total_latency_ns += latency_ns;
    
    if (latency_ns < metrics->min_latency_ns) {
        metrics->min_latency_ns = latency_ns;
    }
    if (latency_ns > metrics->max_latency_ns) {
        metrics->max_latency_ns = latency_ns;
    }
}

static int compare_uint64(const void *a, const void *b) {
    uint64_t va = *(const uint64_t *)a;
    uint64_t vb = *(const uint64_t *)b;
    if (va < vb) return -1;
    if (va > vb) return 1;
    return 0;
}

void benchmark_compute_percentiles(benchmark_metrics_t *metrics) {
    if (metrics->latency_sample_count == 0) return;
    
    qsort(metrics->latency_samples, metrics->latency_sample_count, 
          sizeof(uint64_t), compare_uint64);
    
    size_t p50_idx = metrics->latency_sample_count * 50 / 100;
    size_t p90_idx = metrics->latency_sample_count * 90 / 100;
    size_t p99_idx = metrics->latency_sample_count * 99 / 100;
    
    metrics->p50_latency_ns = metrics->latency_samples[p50_idx];
    metrics->p90_latency_ns = metrics->latency_samples[p90_idx];
    metrics->p99_latency_ns = metrics->latency_samples[p99_idx];
    
    metrics->avg_latency_ns = metrics->total_latency_ns / metrics->latency_sample_count;
}

void benchmark_print_metrics(const benchmark_metrics_t *metrics, const char *protocol_name) {
    printf("\n=== %s Benchmark Results ===\n", protocol_name);
    printf("Commands processed:    %llu\n", (unsigned long long)metrics->commands_processed);
    printf("Bytes processed:       %llu\n", (unsigned long long)metrics->bytes_processed);
    printf("Total time:            %.3f ms\n", metrics->total_time_ns / 1000000.0);
    printf("CPU time:              %.3f ms\n", metrics->cpu_time_us / 1000.0);
    printf("Peak memory:           %llu KB\n", (unsigned long long)metrics->peak_memory_kb);
    
    if (metrics->commands_processed > 0) {
        double throughput = (double)metrics->commands_processed / 
                           (metrics->total_time_ns / 1000000000.0);
        printf("Throughput:            %.0f commands/sec\n", throughput);
        
        double bandwidth_mbps = (double)metrics->bytes_processed * 8 / 
                               (metrics->total_time_ns / 1000.0);
        printf("Bandwidth:             %.2f Mbps\n", bandwidth_mbps);
    }
    
    if (metrics->latency_sample_count > 0) {
        printf("\nLatency (per command):\n");
        printf("  Average:             %.3f μs\n", metrics->avg_latency_ns / 1000.0);
        printf("  Minimum:             %.3f μs\n", metrics->min_latency_ns / 1000.0);
        printf("  Maximum:             %.3f μs\n", metrics->max_latency_ns / 1000.0);
        printf("  P50 (median):        %.3f μs\n", metrics->p50_latency_ns / 1000.0);
        printf("  P90:                 %.3f μs\n", metrics->p90_latency_ns / 1000.0);
        printf("  P99:                 %.3f μs\n", metrics->p99_latency_ns / 1000.0);
    }
    
    printf("\n");
}

void benchmark_print_comparison(const benchmark_metrics_t *resp_metrics,
                               const benchmark_metrics_t *respb_metrics) {
    printf("\n=== RESP vs RESPB Comparison ===\n\n");
    
    // Time comparison
    double time_ratio = (double)resp_metrics->total_time_ns / respb_metrics->total_time_ns;
    double time_savings = (1.0 - 1.0/time_ratio) * 100.0;
    printf("Total Time:\n");
    printf("  RESP:                %.3f ms\n", resp_metrics->total_time_ns / 1000000.0);
    printf("  RESPB:               %.3f ms\n", respb_metrics->total_time_ns / 1000000.0);
    printf("  RESPB is %.2fx faster (%.1f%% time savings)\n\n", time_ratio, time_savings);
    
    // CPU comparison
    double cpu_ratio = (double)resp_metrics->cpu_time_us / respb_metrics->cpu_time_us;
    double cpu_savings = (1.0 - 1.0/cpu_ratio) * 100.0;
    printf("CPU Time:\n");
    printf("  RESP:                %.3f ms\n", resp_metrics->cpu_time_us / 1000.0);
    printf("  RESPB:               %.3f ms\n", respb_metrics->cpu_time_us / 1000.0);
    printf("  RESPB uses %.2fx less CPU (%.1f%% CPU savings)\n\n", cpu_ratio, cpu_savings);
    
    // Bandwidth comparison
    double resp_size_mb = resp_metrics->bytes_processed / (1024.0 * 1024.0);
    double respb_size_mb = respb_metrics->bytes_processed / (1024.0 * 1024.0);
    double size_ratio = (double)resp_metrics->bytes_processed / respb_metrics->bytes_processed;
    double bandwidth_savings = (1.0 - 1.0/size_ratio) * 100.0;
    printf("Wire Size:\n");
    printf("  RESP:                %.3f MB\n", resp_size_mb);
    printf("  RESPB:               %.3f MB\n", respb_size_mb);
    printf("  RESPB is %.2fx smaller (%.1f%% bandwidth savings)\n\n", size_ratio, bandwidth_savings);
    
    // Throughput comparison
    double resp_throughput = (double)resp_metrics->commands_processed / 
                            (resp_metrics->total_time_ns / 1000000000.0);
    double respb_throughput = (double)respb_metrics->commands_processed / 
                             (respb_metrics->total_time_ns / 1000000000.0);
    double throughput_ratio = respb_throughput / resp_throughput;
    printf("Throughput:\n");
    printf("  RESP:                %.0f cmd/s\n", resp_throughput);
    printf("  RESPB:               %.0f cmd/s\n", respb_throughput);
    printf("  RESPB is %.2fx higher throughput\n\n", throughput_ratio);
    
    // Latency comparison
    if (resp_metrics->latency_sample_count > 0 && respb_metrics->latency_sample_count > 0) {
        double lat_ratio = (double)resp_metrics->avg_latency_ns / respb_metrics->avg_latency_ns;
        printf("Average Latency:\n");
        printf("  RESP:                %.3f μs\n", resp_metrics->avg_latency_ns / 1000.0);
        printf("  RESPB:               %.3f μs\n", respb_metrics->avg_latency_ns / 1000.0);
        printf("  RESPB is %.2fx lower latency\n\n", lat_ratio);
    }
    
    // Memory comparison
    printf("Peak Memory:\n");
    printf("  RESP:                %llu KB\n", (unsigned long long)resp_metrics->peak_memory_kb);
    printf("  RESPB:               %llu KB\n", (unsigned long long)respb_metrics->peak_memory_kb);
    if (respb_metrics->peak_memory_kb > 0) {
        double mem_ratio = (double)resp_metrics->peak_memory_kb / respb_metrics->peak_memory_kb;
        printf("  Memory ratio:        %.2fx\n", mem_ratio);
    }
    
    printf("\n");
}

