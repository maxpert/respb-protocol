/*
 * Main Benchmark Runner
 * Orchestrates RESP vs RESPB benchmarks
 */

#include "benchmark.h"
#include "respb.h"
#include "valkey_resp_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int benchmark_resp_parsing(workload_t *wl, benchmark_metrics_t *metrics, 
                                  int iterations, int sample_latency) {
    benchmark_metrics_init(metrics);
    benchmark_timer_t timer;
    
    // Create single client for entire workload  
    valkey_client client;
    valkey_client_init(&client, wl->data, wl->size);
    
    benchmark_timer_start(&timer);
    
    for (int iter = 0; iter < iterations; iter++) {
        // Reset client position for new iteration
        client.qb_pos = 0;
        client.multibulklen = 0;
        client.bulklen = -1;
        client.reqtype = 0;
        client.read_flags = 0;
        
        // Free old argv if any
        if (client.argv) {
            for (int i = 0; i < client.argc; i++) {
                if (client.argv[i]) decrRefCount(client.argv[i]);
            }
            free(client.argv);
            client.argv = NULL;
        }
        client.argc = 0;
        client.argv_len = 0;
        client.argv_len_sum = 0;
        
        while (client.qb_pos < client.querybuf_peak) {
            struct timespec cmd_start, cmd_end;
            if (sample_latency && metrics->latency_sample_count < MAX_LATENCY_SAMPLES) {
                clock_gettime(CLOCK_MONOTONIC, &cmd_start);
            }
            
            size_t start_pos = client.qb_pos;
            int result = valkey_parse_command(&client);
            
            if (sample_latency && metrics->latency_sample_count < MAX_LATENCY_SAMPLES) {
                clock_gettime(CLOCK_MONOTONIC, &cmd_end);
                uint64_t latency_ns = (cmd_end.tv_sec - cmd_start.tv_sec) * 1000000000ULL +
                                     (cmd_end.tv_nsec - cmd_start.tv_nsec);
                benchmark_record_latency(metrics, latency_ns);
            }
            
            if (result == 1) {
                metrics->commands_processed++;
                size_t bytes_consumed = client.qb_pos - start_pos;
                metrics->bytes_processed += bytes_consumed;
                
                // Reset for next command
                for (int i = 0; i < client.argc; i++) {
                    if (client.argv[i]) decrRefCount(client.argv[i]);
                }
                client.argc = 0;
                client.argv_len_sum = 0;
            } else if (result == 0) {
                // Incomplete command, need more data
                break;
            } else {
                // Error
                fprintf(stderr, "RESP parse error at position %zu\n", client.qb_pos);
                valkey_client_free(&client);
                return 0;
            }
        }
    }
    
    benchmark_timer_stop(&timer, metrics);
    benchmark_compute_percentiles(metrics);
    
    valkey_client_free(&client);
    return 1;
}

static int benchmark_respb_parsing(workload_t *wl, benchmark_metrics_t *metrics,
                                   int iterations, int sample_latency) {
    benchmark_metrics_init(metrics);
    benchmark_timer_t timer;
    benchmark_timer_start(&timer);
    
    for (int iter = 0; iter < iterations; iter++) {
        workload_reset(wl);
        
        while (workload_has_more(wl)) {
            respb_parser_t parser;
            respb_parser_init(&parser, wl->data + wl->current_pos,
                            workload_remaining(wl));
            
            respb_command_t cmd;
            
            struct timespec cmd_start, cmd_end;
            if (sample_latency && metrics->latency_sample_count < MAX_LATENCY_SAMPLES) {
                clock_gettime(CLOCK_MONOTONIC, &cmd_start);
            }
            
            int result = respb_parse_command(&parser, &cmd);
            
            if (sample_latency && metrics->latency_sample_count < MAX_LATENCY_SAMPLES) {
                clock_gettime(CLOCK_MONOTONIC, &cmd_end);
                uint64_t latency_ns = (cmd_end.tv_sec - cmd_start.tv_sec) * 1000000000ULL +
                                     (cmd_end.tv_nsec - cmd_start.tv_nsec);
                benchmark_record_latency(metrics, latency_ns);
            }
            
            if (result == 1) {
                metrics->commands_processed++;
                size_t bytes_consumed = parser.pos;
                metrics->bytes_processed += bytes_consumed;
                wl->current_pos += bytes_consumed;
            } else if (result == 0) {
                // Incomplete command - check if we're at end of buffer
                if (parser.pos >= parser.buffer_len || wl->current_pos + parser.pos >= wl->size) {
                    // End of workload
                    break;
                }
                fprintf(stderr, "RESPB incomplete command at position %zu, parser pos %zu, buffer len %zu, opcode 0x%04X\n",
                       wl->current_pos, parser.pos, parser.buffer_len, cmd.opcode);
                fflush(stderr);
                break;
            } else {
                // Error
                fprintf(stderr, "RESPB parse error at position %zu, opcode 0x%04X, parser pos %zu, buffer len %zu\n",
                       wl->current_pos, cmd.opcode, parser.pos, parser.buffer_len);
                fflush(stderr);
                return 0;
            }
        }
    }
    
    benchmark_timer_stop(&timer, metrics);
    benchmark_compute_percentiles(metrics);
    
    return 1;
}

int run_benchmark(benchmark_config_t *config) {
    printf("\n=== Protocol Benchmark Suite ===\n");
    printf("Configuration:\n");
    printf("  Iterations:          %d\n", config->iterations);
    printf("  Sample latency:      %s\n", config->sample_latency ? "Yes" : "No");
    printf("  Workload type:       %s\n", 
           config->workload_type == WORKLOAD_FILE ? "File" :
           config->workload_type == WORKLOAD_SMALL_KEYS ? "Small Keys" :
           config->workload_type == WORKLOAD_MEDIUM_KEYS ? "Medium Keys" :
           config->workload_type == WORKLOAD_LARGE_VALUES ? "Large Values" :
           "Mixed");
    printf("\n");
    
    // Load or generate workloads
    workload_t *resp_workload = NULL;
    workload_t *respb_workload = NULL;
    
    if (config->workload_type == WORKLOAD_FILE) {
        if (config->resp_workload_file) {
            resp_workload = workload_load(config->resp_workload_file);
            if (!resp_workload) {
                fprintf(stderr, "Failed to load RESP workload\n");
                return 0;
            }
        }
        
        if (config->respb_workload_file) {
            respb_workload = workload_load(config->respb_workload_file);
            if (!respb_workload) {
                fprintf(stderr, "Failed to load RESPB workload\n");
                workload_free(resp_workload);
                return 0;
            }
        }
    } else {
        // Generate synthetic workloads
        size_t target_size = 10 * 1024 * 1024; // 10MB
        
        resp_workload = workload_generate_synthetic(target_size, config->workload_type);
        if (!resp_workload) {
            fprintf(stderr, "Failed to generate RESP workload\n");
            return 0;
        }
        
        // For now, use the same RESP workload for RESPB
        // In production, you'd convert RESP to RESPB format
        respb_workload = resp_workload; // Shared for now
    }
    
    // Run RESP benchmark
    if (resp_workload && config->bench_resp) {
        printf("Running RESP benchmark...\n");
        benchmark_metrics_t resp_metrics;
        
        if (!benchmark_resp_parsing(resp_workload, &resp_metrics, 
                                   config->iterations, config->sample_latency)) {
            fprintf(stderr, "RESP benchmark failed\n");
            workload_free(resp_workload);
            if (respb_workload != resp_workload) workload_free(respb_workload);
            return 0;
        }
        
        benchmark_print_metrics(&resp_metrics, "RESP");
        config->resp_metrics = resp_metrics;
    }
    
    // Run RESPB benchmark
    if (respb_workload && config->bench_respb && respb_workload != resp_workload) {
        printf("Running RESPB benchmark...\n");
        benchmark_metrics_t respb_metrics;
        
        if (!benchmark_respb_parsing(respb_workload, &respb_metrics,
                                    config->iterations, config->sample_latency)) {
            fprintf(stderr, "RESPB benchmark failed\n");
            workload_free(resp_workload);
            if (respb_workload != resp_workload) workload_free(respb_workload);
            return 0;
        }
        
        benchmark_print_metrics(&respb_metrics, "RESPB");
        config->respb_metrics = respb_metrics;
    }
    
    // Print comparison if both were run
    if (config->bench_resp && config->bench_respb && respb_workload != resp_workload) {
        benchmark_print_comparison(&config->resp_metrics, &config->respb_metrics);
    }
    
    // Cleanup
    workload_free(resp_workload);
    if (respb_workload != resp_workload) {
        workload_free(respb_workload);
    }
    
    return 1;
}

void print_usage(const char *prog_name) {
    printf("Usage: %s [options]\n", prog_name);
    printf("\nOptions:\n");
    printf("  -r FILE        RESP workload file\n");
    printf("  -b FILE        RESPB workload file\n");
    printf("  -i N           Number of iterations (default: 10)\n");
    printf("  -l             Sample per-command latency\n");
    printf("  -w TYPE        Synthetic workload type:\n");
    printf("                   small   - Small keys (GET)\n");
    printf("                   medium  - Medium keys/values (SET)\n");
    printf("                   large   - Large values (SET)\n");
    printf("                   mixed   - Mixed commands\n");
    printf("  -p PROTOCOL    Benchmark only this protocol (resp|respb|both)\n");
    printf("  -h             Show this help\n");
    printf("\nExamples:\n");
    printf("  %s -w mixed -i 100\n", prog_name);
    printf("  %s -r data/workload_resp.bin -b data/workload_respb.bin -i 50 -l\n", prog_name);
    printf("\n");
}

