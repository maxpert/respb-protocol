/*
 * Protocol Benchmark - Main Entry Point
 * Head-to-head comparison of RESP and RESPB parsing performance
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "benchmark.h"
#include "respb.h"
#include "valkey_resp_parser.h"

int main(int argc, char **argv) {
    benchmark_config_t config = {
        .iterations = 10,
        .sample_latency = 0,
        .bench_resp = 1,
        .bench_respb = 0, // Only RESP for now since we need converted workloads
        .workload_type = WORKLOAD_MIXED,
        .resp_workload_file = NULL,
        .respb_workload_file = NULL
    };
    
    int opt;
    while ((opt = getopt(argc, argv, "r:b:i:lw:p:h")) != -1) {
        switch (opt) {
            case 'r':
                config.resp_workload_file = optarg;
                config.workload_type = WORKLOAD_FILE;
                break;
            case 'b':
                config.respb_workload_file = optarg;
                config.workload_type = WORKLOAD_FILE;
                break;
            case 'i':
                config.iterations = atoi(optarg);
                if (config.iterations <= 0) {
                    fprintf(stderr, "Invalid iterations: %s\n", optarg);
                    return 1;
                }
                break;
            case 'l':
                config.sample_latency = 1;
                break;
            case 'w':
                if (strcmp(optarg, "small") == 0) {
                    config.workload_type = WORKLOAD_SMALL_KEYS;
                } else if (strcmp(optarg, "medium") == 0) {
                    config.workload_type = WORKLOAD_MEDIUM_KEYS;
                } else if (strcmp(optarg, "large") == 0) {
                    config.workload_type = WORKLOAD_LARGE_VALUES;
                } else if (strcmp(optarg, "mixed") == 0) {
                    config.workload_type = WORKLOAD_MIXED;
                } else {
                    fprintf(stderr, "Invalid workload type: %s\n", optarg);
                    return 1;
                }
                break;
            case 'p':
                if (strcmp(optarg, "resp") == 0) {
                    config.bench_resp = 1;
                    config.bench_respb = 0;
                } else if (strcmp(optarg, "respb") == 0) {
                    config.bench_resp = 0;
                    config.bench_respb = 1;
                } else if (strcmp(optarg, "both") == 0) {
                    config.bench_resp = 1;
                    config.bench_respb = 1;
                } else {
                    fprintf(stderr, "Invalid protocol: %s\n", optarg);
                    return 1;
                }
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Run the benchmark
    if (!run_benchmark(&config)) {
        fprintf(stderr, "\nBenchmark failed!\n");
        return 1;
    }
    
    printf("\nBenchmark complete!\n");
    return 0;
}
