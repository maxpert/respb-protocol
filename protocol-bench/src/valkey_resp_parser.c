/*
 * Valkey RESP Parser - Extracted from Valkey networking.c
 * 
 * This file contains the actual production RESP parser from Valkey,
 * with minimal adaptations for the benchmark environment.
 * 
 * Original source: valkey/src/networking.c:parseMultibulk()
 */

#include "valkey_resp_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>

/* ==================== Helper Macros ==================== */

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

/* ==================== Valkey Type Definitions ==================== */

/* Constants from server.h */
#define PROTO_INLINE_MAX_SIZE (1024 * 64)
#define PROTO_MBULK_BIG_ARG (1024 * 32)
#define PROTO_REQ_MULTIBULK 2
#define PROTO_REQ_INLINE 1
#define OBJ_STRING 0

/* Read flags from server.h */
#define READ_FLAGS_ERROR_BIG_MULTIBULK (1 << 2)
#define READ_FLAGS_ERROR_INVALID_MULTIBULK_LEN (1 << 3)
#define READ_FLAGS_ERROR_UNAUTHENTICATED_MULTIBULK_LEN (1 << 4)
#define READ_FLAGS_ERROR_UNAUTHENTICATED_BULK_LEN (1 << 5)
#define READ_FLAGS_ERROR_BIG_BULK_COUNT (1 << 6)
#define READ_FLAGS_ERROR_MBULK_UNEXPECTED_CHARACTER (1 << 7)
#define READ_FLAGS_ERROR_MBULK_INVALID_BULK_LEN (1 << 8)
#define READ_FLAGS_PARSING_NEGATIVE_MBULK_LEN (1 << 12)
#define READ_FLAGS_PARSING_COMPLETED (1 << 13)
#define READ_FLAGS_REPLICATED (1 << 14)
#define READ_FLAGS_AUTH_REQUIRED (1 << 16)

/* ==================== SDS (Simple Dynamic String) Shim ==================== */

/* In Valkey, sds is just char* with length stored before the pointer */
/* For benchmark, we use a simple struct wrapper */

struct sdshdr {
    size_t len;
    size_t alloc;
    char buf[];
};

#define SDS_HDR(s) ((struct sdshdr*)((s) - sizeof(struct sdshdr)))
#define SDS_NOINIT 0

static inline size_t sdslen(const sds s) {
    if (s == NULL) return 0;
    return SDS_HDR(s)->len;
}

static inline size_t sdsavail(const sds s) {
    if (s == NULL) return 0;
    struct sdshdr *sh = SDS_HDR(s);
    return sh->alloc - sh->len;
}

static inline size_t sdsalloc(const sds s) {
    if (s == NULL) return 0;
    return SDS_HDR(s)->alloc;
}

sds sdsnewlen(const void *init, size_t initlen) {
    struct sdshdr *sh = malloc(sizeof(struct sdshdr) + initlen + 1);
    if (sh == NULL) return NULL;
    
    sh->len = initlen;
    sh->alloc = initlen;
    
    if (init && initlen) {
        memcpy(sh->buf, init, initlen);
    }
    sh->buf[initlen] = '\0';
    
    return sh->buf;
}

sds sdsempty(void) {
    return sdsnewlen(NULL, 0);
}

void sdsfree(sds s) {
    if (s == NULL) return;
    free(SDS_HDR(s));
}

void sdsclear(sds s) {
    if (s == NULL) return;
    struct sdshdr *sh = SDS_HDR(s);
    sh->len = 0;
    sh->buf[0] = '\0';
}

sds sdsMakeRoomFor(sds s, size_t addlen) {
    if (s == NULL) {
        return sdsnewlen(NULL, addlen);
    }
    
    size_t avail = sdsavail(s);
    if (avail >= addlen) return s;
    
    struct sdshdr *sh = SDS_HDR(s);
    size_t newlen = sh->len + addlen;
    size_t newalloc = newlen * 2; // Greedy growth
    
    struct sdshdr *newsh = realloc(sh, sizeof(struct sdshdr) + newalloc + 1);
    if (newsh == NULL) return NULL;
    
    newsh->alloc = newalloc;
    return newsh->buf;
}

sds sdsMakeRoomForNonGreedy(sds s, size_t addlen) {
    if (s == NULL) {
        return sdsnewlen(NULL, addlen);
    }
    
    size_t avail = sdsavail(s);
    if (avail >= addlen) return s;
    
    struct sdshdr *sh = SDS_HDR(s);
    size_t newlen = sh->len + addlen;
    
    struct sdshdr *newsh = realloc(sh, sizeof(struct sdshdr) + newlen + 1);
    if (newsh == NULL) return NULL;
    
    newsh->alloc = newlen;
    return newsh->buf;
}

void sdsIncrLen(sds s, ssize_t incr) {
    if (s == NULL) return;
    struct sdshdr *sh = SDS_HDR(s);
    sh->len += incr;
    sh->buf[sh->len] = '\0';
}

void sdsrange(sds s, ssize_t start, ssize_t end) {
    if (s == NULL) return;
    struct sdshdr *sh = SDS_HDR(s);
    size_t len = sh->len;
    
    if (len == 0) return;
    if (start < 0) start = len + start;
    if (end < 0) end = len + end;
    if (start < 0) start = 0;
    if (end < 0) end = 0;
    if ((size_t)end >= len) end = len - 1;
    
    size_t newlen = (start > end) ? 0 : (end - start + 1);
    if (newlen != 0) {
        if ((size_t)start != 0) memmove(sh->buf, sh->buf + start, newlen);
    }
    sh->len = newlen;
    sh->buf[newlen] = '\0';
}

/* ==================== Redis Object (robj) Shim ==================== */

robj *createObject(int type, void *ptr) {
    robj *o = malloc(sizeof(robj));
    if (o == NULL) return NULL;
    
    o->type = type;
    o->encoding = 0;
    o->ptr = ptr;
    o->refcount = 1;
    
    return o;
}

robj *createStringObject(const char *ptr, size_t len) {
    sds s = sdsnewlen(ptr, len);
    if (s == NULL) return NULL;
    return createObject(OBJ_STRING, s);
}

void decrRefCount(robj *o) {
    if (o == NULL) return;
    
    if (--o->refcount == 0) {
        if (o->type == OBJ_STRING) {
            sdsfree(o->ptr);
        }
        free(o);
    }
}

void incrRefCount(robj *o) {
    if (o) o->refcount++;
}

/* ==================== Memory Allocation Shims ==================== */

#define zmalloc malloc
#define zrealloc realloc
#define zfree free

/* ==================== Utility Functions ==================== */

/* string2ll: Convert string to long long (from util.c) */
int string2ll(const char *s, size_t slen, long long *value) {
    const char *p = s;
    size_t plen = 0;
    int negative = 0;
    unsigned long long v;

    if (plen == slen) return 0;

    /* Special case: first and only digit is 0 */
    if (slen == 1 && p[0] == '0') {
        if (value != NULL) *value = 0;
        return 1;
    }

    if (p[0] == '-') {
        negative = 1;
        p++;
        plen++;

        /* Abort on only a negative sign */
        if (plen == slen) return 0;
    }

    /* First digit should be 1-9, otherwise the string should just be 0 */
    if (p[0] >= '1' && p[0] <= '9') {
        v = p[0] - '0';
        p++;
        plen++;
    } else if (p[0] == '0' && slen == 1) {
        *value = 0;
        return 1;
    } else {
        return 0;
    }

    while (plen < slen && p[0] >= '0' && p[0] <= '9') {
        if (v > (ULLONG_MAX / 10)) /* Overflow */
            return 0;
        v *= 10;

        if (v > (ULLONG_MAX - (p[0] - '0'))) /* Overflow */
            return 0;
        v += p[0] - '0';

        p++;
        plen++;
    }

    /* Return if not all bytes were used */
    if (plen < slen) return 0;

    if (negative) {
        if (v > ((unsigned long long)(-(LLONG_MIN + 1)) + 1)) /* Overflow */
            return 0;
        if (value != NULL) *value = -v;
    } else {
        if (v > LLONG_MAX) /* Overflow */
            return 0;
        if (value != NULL) *value = v;
    }
    return 1;
}

/* ==================== Valkey RESP Parser (from networking.c) ==================== */

/* 
 * Incremental parsing of a command in the client's query buffer.
 *
 * This is the ACTUAL production parser from Valkey's networking.c,
 * extracted with minimal modifications.
 *
 * Returns a non-zero if parsing is complete (either error or success) and zero
 * if the input buffer doesn't contain enough data to parse a complete
 * command. If non-zero is returned, the returned value is a read flag, either
 * READ_FLAGS_PARSING_COMPLETED on success or one of the READ_FLAGS_ERROR_(...)
 * values on parse error.
 */
static int parseMultibulk(valkey_client *c,
                          int *argc,
                          robj ***argv,
                          int *argv_len,
                          size_t *argv_len_sum,
                          unsigned long long *net_input_bytes_curr_cmd) {
    char *newline = NULL;
    int ok;
    long long ll;
    int is_replicated = c->read_flags & READ_FLAGS_REPLICATED;
    int auth_required = c->read_flags & READ_FLAGS_AUTH_REQUIRED;

    if (c->multibulklen == 0) {
        /* The client (argc) should have been reset */
        assert(*argc == 0);

        /* Multi bulk length cannot be read without a \r\n */
        newline = memchr(c->querybuf + c->qb_pos, '\r', sdslen(c->querybuf) - c->qb_pos);
        if (newline == NULL) {
            if (sdslen(c->querybuf) - c->qb_pos > PROTO_INLINE_MAX_SIZE) {
                return READ_FLAGS_ERROR_BIG_MULTIBULK;
            }
            return 0;
        }

        /* Buffer should also contain \n */
        if (newline - (c->querybuf + c->qb_pos) > (ssize_t)(sdslen(c->querybuf) - c->qb_pos - 2)) return 0;

        /* We know for sure there is a whole line since newline != NULL,
         * so go ahead and find out the multi bulk length. */
        assert(c->querybuf[c->qb_pos] == '*');
        size_t multibulklen_slen = newline - (c->querybuf + 1 + c->qb_pos);
        ok = string2ll(c->querybuf + 1 + c->qb_pos, multibulklen_slen, &ll);
        if (!ok || ll > INT_MAX) {
            return READ_FLAGS_ERROR_INVALID_MULTIBULK_LEN;
        } else if (ll > 10 && auth_required) {
            return READ_FLAGS_ERROR_UNAUTHENTICATED_MULTIBULK_LEN;
        }

        c->qb_pos = (newline - c->querybuf) + 2;

        if (ll <= 0) {
            return READ_FLAGS_PARSING_NEGATIVE_MBULK_LEN;
        }

        c->multibulklen = ll;
        c->bulklen = -1;

        /* Setup argv array */
        if (*argv) zfree(*argv);
        *argv_len = ll < 1024 ? ll : 1024;
        *argv = zmalloc(sizeof(robj *) * *argv_len);
        *argv_len_sum = 0;

        *net_input_bytes_curr_cmd += (multibulklen_slen + 3);
    }

    assert(c->multibulklen > 0);
    while (c->multibulklen) {
        /* Read bulk length if unknown */
        if (c->bulklen == -1) {
            newline = memchr(c->querybuf + c->qb_pos, '\r', sdslen(c->querybuf) - c->qb_pos);
            if (newline == NULL) {
                if (sdslen(c->querybuf) - c->qb_pos > PROTO_INLINE_MAX_SIZE) {
                    return READ_FLAGS_ERROR_BIG_BULK_COUNT;
                }
                break;
            }

            /* Buffer should also contain \n */
            if (newline - (c->querybuf + c->qb_pos) > (ssize_t)(sdslen(c->querybuf) - c->qb_pos - 2)) return 0;

            if (c->querybuf[c->qb_pos] != '$') {
                return READ_FLAGS_ERROR_MBULK_UNEXPECTED_CHARACTER;
            }

            size_t bulklen_slen = newline - (c->querybuf + c->qb_pos + 1);
            ok = string2ll(c->querybuf + c->qb_pos + 1, bulklen_slen, &ll);
            if (!ok || ll < 0 || (!(is_replicated) && ll > (512 * 1024 * 1024))) { // 512MB max
                return READ_FLAGS_ERROR_MBULK_INVALID_BULK_LEN;
            } else if (ll > 16384 && auth_required) {
                return READ_FLAGS_ERROR_UNAUTHENTICATED_BULK_LEN;
            }

            c->qb_pos = newline - c->querybuf + 2;
            if (!(is_replicated) && ll >= PROTO_MBULK_BIG_ARG) {
                if (sdslen(c->querybuf) - c->qb_pos <= (size_t)ll + 2) {
                    sdsrange(c->querybuf, c->qb_pos, -1);
                    c->qb_pos = 0;
                    c->querybuf = sdsMakeRoomForNonGreedy(c->querybuf, ll + 2 - sdslen(c->querybuf));
                    if (c->querybuf_peak < (size_t)ll + 2) c->querybuf_peak = ll + 2;
                }
            }
            c->bulklen = ll;
            *net_input_bytes_curr_cmd += (bulklen_slen + 3);
        }

        /* Read bulk argument */
        if (sdslen(c->querybuf) - c->qb_pos < (size_t)(c->bulklen + 2)) {
            /* Not enough data (+2 == trailing \r\n) */
            break;
        } else {
            /* Check if we have space in argv, grow if needed */
            if (*argc >= *argv_len) {
                *argv_len = (*argv_len < INT_MAX / 2) ? (*argv_len) * 2 : INT_MAX;
                if (*argv_len < *argc + c->multibulklen) {
                    *argv_len = *argc + c->multibulklen;
                }
                *argv = zrealloc(*argv, sizeof(robj *) * (*argv_len));
            }

            /* Optimization: if a non-replicated client's buffer contains JUST our bulk element
             * instead of creating a new object by *copying* the sds we
             * just use the current sds string. */
            if (!is_replicated && c->qb_pos == 0 && c->bulklen >= PROTO_MBULK_BIG_ARG &&
                sdslen(c->querybuf) == (size_t)(c->bulklen + 2)) {
                (*argv)[(*argc)++] = createObject(OBJ_STRING, c->querybuf);
                *argv_len_sum += c->bulklen;
                sdsIncrLen(c->querybuf, -2); /* remove CRLF */
                /* Assume that if we saw a fat argument we'll see another one
                 * likely... */
                c->querybuf = sdsnewlen(NULL, c->bulklen + 2);
                sdsclear(c->querybuf);
            } else {
                (*argv)[(*argc)++] = createStringObject(c->querybuf + c->qb_pos, c->bulklen);
                *argv_len_sum += c->bulklen;
                c->qb_pos += c->bulklen + 2;
            }
            c->bulklen = -1;
            c->multibulklen--;
        }
    }

    /* We're done when c->multibulklen == 0 */
    if (c->multibulklen == 0) {
        *net_input_bytes_curr_cmd += (*argv_len_sum + (*argc * 2));
        c->reqtype = 0;
        return READ_FLAGS_PARSING_COMPLETED;
    }
    return 0;
}

/* ==================== Public API ==================== */

void valkey_client_init(valkey_client *c, const uint8_t *buf, size_t len) {
    memset(c, 0, sizeof(valkey_client));
    
    // Create sds from buffer
    c->querybuf = sdsnewlen(buf, len);
    c->qb_pos = 0;
    c->multibulklen = 0;
    c->bulklen = -1;
    c->reqtype = 0;
    c->read_flags = 0;
    c->argc = 0;
    c->argv = NULL;
    c->argv_len = 0;
    c->argv_len_sum = 0;
    c->querybuf_peak = len;
    c->net_input_bytes_curr_cmd = 0;
}

void valkey_client_free(valkey_client *c) {
    if (c->querybuf) {
        sdsfree(c->querybuf);
        c->querybuf = NULL;
    }
    
    if (c->argv) {
        for (int i = 0; i < c->argc; i++) {
            decrRefCount(c->argv[i]);
        }
        zfree(c->argv);
        c->argv = NULL;
    }
}

int valkey_parse_command(valkey_client *c) {
    /* Determine request type when unknown */
    if (!c->reqtype) {
        if (c->qb_pos >= sdslen(c->querybuf)) {
            return 0; // No data
        }
        
        if (c->querybuf[c->qb_pos] == '*') {
            c->reqtype = PROTO_REQ_MULTIBULK;
        } else {
            c->reqtype = PROTO_REQ_INLINE;
        }
    }

    if (c->reqtype == PROTO_REQ_MULTIBULK) {
        int flag = parseMultibulk(c, &c->argc, &c->argv, &c->argv_len,
                                  &c->argv_len_sum, &c->net_input_bytes_curr_cmd);
        c->read_flags |= flag;
        
        if (flag & READ_FLAGS_PARSING_COMPLETED) {
            return 1; // Success
        } else if (flag & (READ_FLAGS_ERROR_BIG_MULTIBULK | 
                          READ_FLAGS_ERROR_INVALID_MULTIBULK_LEN |
                          READ_FLAGS_ERROR_BIG_BULK_COUNT |
                          READ_FLAGS_ERROR_MBULK_UNEXPECTED_CHARACTER |
                          READ_FLAGS_ERROR_MBULK_INVALID_BULK_LEN)) {
            return -1; // Error
        }
        
        return 0; // Incomplete
    }
    
    // We don't handle inline protocol for benchmark
    return -1;
}

const char *valkey_command_name(const valkey_client *c) {
    static char name_buf[64];
    
    if (c->argc == 0 || c->argv == NULL || c->argv[0] == NULL) {
        return "UNKNOWN";
    }
    
    robj *cmd = c->argv[0];
    sds s = (sds)cmd->ptr;
    size_t len = sdslen(s);
    
    if (len >= sizeof(name_buf)) {
        len = sizeof(name_buf) - 1;
    }
    
    for (size_t i = 0; i < len; i++) {
        name_buf[i] = toupper(s[i]);
    }
    name_buf[len] = '\0';
    
    return name_buf;
}

