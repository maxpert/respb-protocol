/*
 * Valkey RESP Parser API
 * 
 * This is Valkey's production RESP parser extracted from networking.c
 * with minimal adaptations for standalone use in the benchmark.
 */

#ifndef VALKEY_RESP_PARSER_H
#define VALKEY_RESP_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <ctype.h>

/* ==================== Type Definitions ==================== */

/* SDS - Simple Dynamic String (Valkey's string type) */
typedef char *sds;

/* Redis Object */
typedef struct robj {
    unsigned type : 4;
    unsigned encoding : 4;
    int refcount;
    void *ptr;
} robj;

/* Simplified client structure containing only fields needed for parsing */
typedef struct valkey_client {
    /* Input buffer and parsing state */
    sds querybuf;                     /* Buffer containing the query */
    size_t qb_pos;                    /* Current position in querybuf */
    int multibulklen;                 /* Number of multi bulk arguments left */
    long bulklen;                     /* Length of current bulk argument */
    int reqtype;                      /* Request type (multibulk/inline) */
    int read_flags;                   /* Read flags for parser state */
    
    /* Parsed command */
    robj **argv;                      /* Parsed arguments */
    int argc;                         /* Number of arguments */
    int argv_len;                     /* Size of argv array */
    size_t argv_len_sum;              /* Sum of argument lengths */
    
    /* Metrics */
    size_t querybuf_peak;             /* Peak querybuf size */
    unsigned long long net_input_bytes_curr_cmd; /* Bytes for current command */
} valkey_client;

/* ==================== Public API ==================== */

/**
 * Initialize a Valkey client parser with a buffer
 */
void valkey_client_init(valkey_client *c, const uint8_t *buf, size_t len);

/**
 * Free resources associated with a client
 */
void valkey_client_free(valkey_client *c);

/**
 * Parse a RESP command from the client's buffer
 * 
 * Returns:
 *   1  - Command parsed successfully
 *   0  - Incomplete command (need more data)
 *  -1  - Parse error
 */
int valkey_parse_command(valkey_client *c);

/**
 * Get the command name from a parsed command
 */
const char *valkey_command_name(const valkey_client *c);

/* ==================== SDS Functions (for external use if needed) ==================== */

sds sdsnewlen(const void *init, size_t initlen);
sds sdsempty(void);
void sdsfree(sds s);
void sdsclear(sds s);
sds sdsMakeRoomFor(sds s, size_t addlen);
sds sdsMakeRoomForNonGreedy(sds s, size_t addlen);
void sdsIncrLen(sds s, ssize_t incr);
void sdsrange(sds s, ssize_t start, ssize_t end);

/* ==================== Redis Object Functions ==================== */

robj *createObject(int type, void *ptr);
robj *createStringObject(const char *ptr, size_t len);
void decrRefCount(robj *o);
void incrRefCount(robj *o);

/* ==================== Utility Functions ==================== */

int string2ll(const char *s, size_t slen, long long *value);

#endif // VALKEY_RESP_PARSER_H

