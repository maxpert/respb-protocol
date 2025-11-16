/*
 * Workload Management
 * Handles loading and managing binary workload files
 */

#include "benchmark.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

workload_t *workload_load(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open workload file: %s\n", filename);
        return NULL;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size <= 0 || file_size > 1024 * 1024 * 1024) { // Max 1GB
        fprintf(stderr, "Invalid workload file size: %ld bytes\n", file_size);
        fclose(f);
        return NULL;
    }
    
    workload_t *wl = (workload_t *)malloc(sizeof(workload_t));
    if (!wl) {
        fclose(f);
        return NULL;
    }
    
    wl->data = (uint8_t *)malloc(file_size);
    if (!wl->data) {
        free(wl);
        fclose(f);
        return NULL;
    }
    
    size_t read_bytes = fread(wl->data, 1, file_size, f);
    fclose(f);
    
    if (read_bytes != (size_t)file_size) {
        fprintf(stderr, "Failed to read workload file completely\n");
        free(wl->data);
        free(wl);
        return NULL;
    }
    
    wl->size = file_size;
    wl->current_pos = 0;
    
    printf("Loaded workload: %s (%zu bytes)\n", filename, wl->size);
    return wl;
}

void workload_free(workload_t *wl) {
    if (wl) {
        free(wl->data);
        free(wl);
    }
}

void workload_reset(workload_t *wl) {
    wl->current_pos = 0;
}

int workload_has_more(const workload_t *wl) {
    return wl->current_pos < wl->size;
}

size_t workload_remaining(const workload_t *wl) {
    return wl->size - wl->current_pos;
}

// Generate synthetic workload in memory
workload_t *workload_generate_synthetic(size_t target_size, workload_type_t type) {
    workload_t *wl = (workload_t *)malloc(sizeof(workload_t));
    if (!wl) return NULL;
    
    wl->data = (uint8_t *)malloc(target_size);
    if (!wl->data) {
        free(wl);
        return NULL;
    }
    
    wl->size = 0;
    wl->current_pos = 0;
    
    uint8_t temp_buf[4096];
    
    switch (type) {
        case WORKLOAD_SMALL_KEYS: {
            // Generate many small GET commands
            while (wl->size + 100 < target_size) {
                // RESP format: *2\r\n$3\r\nGET\r\n$6\r\nkey_XX\r\n
                int len = snprintf((char *)temp_buf, sizeof(temp_buf),
                    "*2\r\n$3\r\nGET\r\n$6\r\nkey_%02zu\r\n",
                    wl->size % 100);
                
                if (wl->size + len > target_size) break;
                memcpy(wl->data + wl->size, temp_buf, len);
                wl->size += len;
            }
            break;
        }
        
        case WORKLOAD_MEDIUM_KEYS: {
            // Generate SET commands with medium values
            const char *value_50 = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"; // 50 bytes
            while (wl->size + 200 < target_size) {
                // key_0000 is 8 bytes
                int len = snprintf((char *)temp_buf, sizeof(temp_buf),
                    "*3\r\n$3\r\nSET\r\n$8\r\nkey_%04zu\r\n$50\r\n%s\r\n",
                    wl->size % 1000, value_50);
                
                if (wl->size + len > target_size) break;
                memcpy(wl->data + wl->size, temp_buf, len);
                wl->size += len;
            }
            break;
        }
        
        case WORKLOAD_LARGE_VALUES: {
            // Generate SET commands with large values (1KB)
            char large_value[1024];
            memset(large_value, 'X', sizeof(large_value));
            
            while (wl->size + 1100 < target_size) {
                int header_len = snprintf((char *)temp_buf, sizeof(temp_buf),
                    "*3\r\n$3\r\nSET\r\n$9\r\nlargekey%zu\r\n$1024\r\n",
                    wl->size % 100);
                
                if (wl->size + header_len + 1024 + 2 > target_size) break;
                
                memcpy(wl->data + wl->size, temp_buf, header_len);
                wl->size += header_len;
                memcpy(wl->data + wl->size, large_value, 1024);
                wl->size += 1024;
                memcpy(wl->data + wl->size, "\r\n", 2);
                wl->size += 2;
            }
            break;
        }
        
        case WORKLOAD_MIXED: {
            // Mix of GET, SET, MGET, DEL
            size_t cmd_count = 0;
            while (wl->size + 200 < target_size) {
                int choice = cmd_count % 4;
                int len = 0;
                
                switch (choice) {
                    case 0: // GET - key_XX is 6 bytes
                        len = snprintf((char *)temp_buf, sizeof(temp_buf),
                            "*2\r\n$3\r\nGET\r\n$6\r\nkey_%02zu\r\n", cmd_count % 100);
                        break;
                    case 1: // SET - key_XX and val_XX are each 6 bytes
                        len = snprintf((char *)temp_buf, sizeof(temp_buf),
                            "*3\r\n$3\r\nSET\r\n$6\r\nkey_%02zu\r\n$6\r\nval_%02zu\r\n",
                            cmd_count % 100, cmd_count % 100);
                        break;
                    case 2: // DEL - key_XX is 6 bytes
                        len = snprintf((char *)temp_buf, sizeof(temp_buf),
                            "*2\r\n$3\r\nDEL\r\n$6\r\nkey_%02zu\r\n", cmd_count % 100);
                        break;
                    case 3: // MGET - key_0, key_1, key_2 are each 5 bytes
                        len = snprintf((char *)temp_buf, sizeof(temp_buf),
                            "*4\r\n$4\r\nMGET\r\n$5\r\nkey_0\r\n$5\r\nkey_1\r\n$5\r\nkey_2\r\n");
                        break;
                }
                
                if (wl->size + len > target_size) break;
                memcpy(wl->data + wl->size, temp_buf, len);
                wl->size += len;
                cmd_count++;
            }
            break;
        }
        
        default:
            free(wl->data);
            free(wl);
            return NULL;
    }
    
    printf("Generated synthetic %s workload: %zu bytes\n",
           type == WORKLOAD_SMALL_KEYS ? "SMALL_KEYS" :
           type == WORKLOAD_MEDIUM_KEYS ? "MEDIUM_KEYS" :
           type == WORKLOAD_LARGE_VALUES ? "LARGE_VALUES" : "MIXED",
           wl->size);
    
    return wl;
}

// Save workload to file
int workload_save(const workload_t *wl, const char *filename) {
    FILE *f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Failed to open file for writing: %s\n", filename);
        return 0;
    }
    
    size_t written = fwrite(wl->data, 1, wl->size, f);
    fclose(f);
    
    if (written != wl->size) {
        fprintf(stderr, "Failed to write complete workload\n");
        return 0;
    }
    
    printf("Saved workload to: %s (%zu bytes)\n", filename, wl->size);
    return 1;
}

