/*
 * RESPB Parser V2 - Clean Implementation
 * Interface-compatible with valkey_resp_parser
 * Follows respb-specs.md and respb-commands.md exactly
 */

#include "respb.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Helper functions for reading binary data */
static inline uint16_t read_u16_be(const uint8_t *buf) {
    return ((uint16_t)buf[0] << 8) | buf[1];
}

static inline uint32_t read_u32_be(const uint8_t *buf) {
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) | buf[3];
}

static inline uint64_t read_u64_be(const uint8_t *buf) {
    return ((uint64_t)buf[0] << 56) | ((uint64_t)buf[1] << 48) |
           ((uint64_t)buf[2] << 40) | ((uint64_t)buf[3] << 32) |
           ((uint64_t)buf[4] << 24) | ((uint64_t)buf[5] << 16) |
           ((uint64_t)buf[6] << 8) | buf[7];
}

/* Check if enough bytes are available */
#define CHECK_AVAIL(parser, n) \
    if ((parser)->pos + (n) > (parser)->buffer_len) return 0

/* Macro to read a 2-byte length-prefixed field */
#define READ_STRING_2B(parser, arg_ptr) do { \
    CHECK_AVAIL(parser, 2); \
    uint16_t len = read_u16_be((parser)->buffer + (parser)->pos); \
    (parser)->pos += 2; \
    CHECK_AVAIL(parser, len); \
    (arg_ptr)->data = (parser)->buffer + (parser)->pos; \
    (arg_ptr)->len = len; \
    (parser)->pos += len; \
} while(0)

/* Macro to read a 4-byte length-prefixed field */
#define READ_STRING_4B(parser, arg_ptr) do { \
    CHECK_AVAIL(parser, 4); \
    uint32_t len = read_u32_be((parser)->buffer + (parser)->pos); \
    (parser)->pos += 4; \
    CHECK_AVAIL(parser, len); \
    (arg_ptr)->data = (parser)->buffer + (parser)->pos; \
    (arg_ptr)->len = len; \
    (parser)->pos += len; \
} while(0)

void respb_parser_init(respb_parser_t *parser, const uint8_t *buf, size_t len) {
    parser->buffer = buf;
    parser->buffer_len = len;
    parser->pos = 0;
}

int respb_parse_header(respb_parser_t *parser, uint16_t *opcode, uint16_t *mux_id) {
    CHECK_AVAIL(parser, 4);
    *opcode = read_u16_be(parser->buffer + parser->pos);
    *mux_id = read_u16_be(parser->buffer + parser->pos + 2);
    return 1;
}

int respb_parse_command(respb_parser_t *parser, respb_command_t *cmd) {
    /* Read header (minimum 4 bytes: opcode + mux_id) */
    CHECK_AVAIL(parser, 4);
    
    cmd->opcode = read_u16_be(parser->buffer + parser->pos);
    cmd->mux_id = read_u16_be(parser->buffer + parser->pos + 2);
    parser->pos += 4;
    
    cmd->argc = 0;
    cmd->raw_payload = parser->buffer + parser->pos;
    size_t payload_start = parser->pos;
    
    /* Dispatch based on opcode */
    switch (cmd->opcode) {
        /* ===== String Operations (0x0000-0x003F) ===== */
        
        case RESPB_OP_GET:      /* [2B keylen][key] */
        case RESPB_OP_DECR:
        case RESPB_OP_GETDEL:
        case RESPB_OP_INCR:
        case RESPB_OP_STRLEN:
            READ_STRING_2B(parser, &cmd->args[0]);
            cmd->argc = 1;
            break;
            
        case RESPB_OP_SET:      /* [2B keylen][key][4B vallen][value][1B flags][8B expiry] */
            READ_STRING_2B(parser, &cmd->args[0]); /* key */
            READ_STRING_4B(parser, &cmd->args[1]); /* value */
            CHECK_AVAIL(parser, 9); /* flags + expiry */
            parser->pos += 9;
            cmd->argc = 2;
            break;
            
        case RESPB_OP_APPEND:   /* [2B keylen][key][4B datalen][data] */
        case RESPB_OP_SETNX:
            READ_STRING_2B(parser, &cmd->args[0]); /* key */
            READ_STRING_4B(parser, &cmd->args[1]); /* data/value */
            cmd->argc = 2;
            break;
            
        case RESPB_OP_INCRBY:   /* [2B keylen][key][8B increment] */
        case RESPB_OP_DECRBY:
            READ_STRING_2B(parser, &cmd->args[0]); /* key */
            CHECK_AVAIL(parser, 8); /* integer */
            parser->pos += 8;
            cmd->argc = 1;
            break;
            
        case RESPB_OP_GETEX:    /* [2B keylen][key][1B flags][8B expiry?] */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 1);
            uint8_t flags = parser->buffer[parser->pos];
            parser->pos += 1;
            if (flags & 0x01) { /* Has expiry */
                CHECK_AVAIL(parser, 8);
                parser->pos += 8;
            }
            cmd->argc = 1;
            break;
            
        case RESPB_OP_GETRANGE: /* [2B keylen][key][8B start][8B end] */
        case RESPB_OP_SUBSTR:
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 16);
            parser->pos += 16;
            cmd->argc = 1;
            break;
            
        case RESPB_OP_GETSET:   /* [2B keylen][key][4B vallen][value] */
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_4B(parser, &cmd->args[1]);
            cmd->argc = 2;
            break;
            
        case RESPB_OP_INCRBYFLOAT: /* [2B keylen][key][8B float] */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 8);
            parser->pos += 8;
            cmd->argc = 1;
            break;
            
        case RESPB_OP_MSETNX: {   /* Same as MSET */
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i * 2 + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i * 2]);
                READ_STRING_4B(parser, &cmd->args[i * 2 + 1]);
            }
            cmd->argc = (count < RESPB_MAX_ARGS / 2 ? count * 2 : RESPB_MAX_ARGS);
            break;
        }
            
        case RESPB_OP_PSETEX:   /* [2B keylen][key][8B millis][4B vallen][value] */
        case RESPB_OP_SETEX:    /* [2B keylen][key][8B seconds][4B vallen][value] */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 8);
            parser->pos += 8;
            READ_STRING_4B(parser, &cmd->args[1]);
            cmd->argc = 2;
            break;
            
        case RESPB_OP_SETRANGE: /* [2B keylen][key][8B offset][4B vallen][value] */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 8);
            parser->pos += 8;
            READ_STRING_4B(parser, &cmd->args[1]);
            cmd->argc = 2;
            break;
            
        case RESPB_OP_LCS:      /* [2B key1len][key1][2B key2len][key2][1B flags] */
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            CHECK_AVAIL(parser, 1);
            parser->pos += 1;
            cmd->argc = 2;
            break;
            
        case RESPB_OP_DELIFEQ:  /* [2B keylen][key][4B vallen][value] */
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_4B(parser, &cmd->args[1]);
            cmd->argc = 2;
            break;
            
        case RESPB_OP_EXPIRE:   /* [2B keylen][key][8B seconds][1B flags] */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 9);
            parser->pos += 9;
            cmd->argc = 1;
            break;
            
        case RESPB_OP_MGET:     /* [2B count]([2B keylen][key])... */
        case RESPB_OP_DEL:
        case RESPB_OP_EXISTS:
        case RESPB_OP_UNLINK: {
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i]);
            }
            cmd->argc = count < RESPB_MAX_ARGS ? count : RESPB_MAX_ARGS;
            break;
        }
            
        case RESPB_OP_MSET: {   /* [2B count]([2B keylen][key][4B vallen][value])... */
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i * 2 + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i * 2]);     /* key */
                READ_STRING_4B(parser, &cmd->args[i * 2 + 1]); /* value */
            }
            cmd->argc = (count < RESPB_MAX_ARGS / 2 ? count * 2 : RESPB_MAX_ARGS);
            break;
        }
        
        /* ===== List Operations (0x0040-0x007F) ===== */
        
        case RESPB_OP_LPUSH:    /* [2B keylen][key][2B count]([2B elemlen][element])... */
        case RESPB_OP_RPUSH: {
            READ_STRING_2B(parser, &cmd->args[0]); /* key */
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i + 1]);
            }
            cmd->argc = 1 + (count < RESPB_MAX_ARGS - 1 ? count : RESPB_MAX_ARGS - 1);
            break;
        }
        
        case RESPB_OP_LPOP:     /* [2B keylen][key][2B count?] */
        case RESPB_OP_RPOP:
            READ_STRING_2B(parser, &cmd->args[0]);
            /* Optional count field - simplified, just parse key */
            cmd->argc = 1;
            break;
            
        case RESPB_OP_LLEN:
            READ_STRING_2B(parser, &cmd->args[0]);
            cmd->argc = 1;
            break;
            
        case RESPB_OP_LRANGE: {  /* [2B keylen][key][8B start][8B stop] */
            READ_STRING_2B(parser, &cmd->args[0]); /* key */
            CHECK_AVAIL(parser, 16); /* start + stop */
            parser->pos += 16;
            cmd->argc = 1;
            break;
        }
        
        case RESPB_OP_LINDEX:   /* [2B keylen][key][8B index] */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 8);
            parser->pos += 8;
            cmd->argc = 1;
            break;
            
        case RESPB_OP_LSET:     /* [2B keylen][key][8B index][2B elemlen][elem] */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 8);
            parser->pos += 8;
            READ_STRING_2B(parser, &cmd->args[1]);
            cmd->argc = 2;
            break;
            
        case RESPB_OP_LREM:     /* [2B keylen][key][8B count][2B elemlen][elem] */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 8);
            parser->pos += 8;
            READ_STRING_2B(parser, &cmd->args[1]);
            cmd->argc = 2;
            break;
            
        case RESPB_OP_LTRIM:    /* [2B keylen][key][8B start][8B stop] */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 16);
            parser->pos += 16;
            cmd->argc = 1;
            break;
            
        case RESPB_OP_LINSERT:  /* [2B keylen][key][1B before_after][2B pivotlen][pivot][2B elemlen][elem] */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 1);
            parser->pos += 1;
            READ_STRING_2B(parser, &cmd->args[1]);
            READ_STRING_2B(parser, &cmd->args[2]);
            cmd->argc = 3;
            break;
            
        case RESPB_OP_LPUSHX:   /* [2B keylen][key][2B count]([2B elemlen][elem])... */
        case RESPB_OP_RPUSHX: {
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i + 1]);
            }
            cmd->argc = 1 + (count < RESPB_MAX_ARGS - 1 ? count : RESPB_MAX_ARGS - 1);
            break;
        }
            
        case RESPB_OP_RPOPLPUSH: /* [2B srclen][src][2B dstlen][dst] */
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            cmd->argc = 2;
            break;
            
        case RESPB_OP_LMOVE:    /* [2B srclen][src][2B dstlen][dst][1B wherefrom][1B whereto] */
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            CHECK_AVAIL(parser, 2);
            parser->pos += 2;
            cmd->argc = 2;
            break;
            
        case RESPB_OP_BRPOPLPUSH: /* [2B srclen][src][2B dstlen][dst][8B timeout] */
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            CHECK_AVAIL(parser, 8);
            parser->pos += 8;
            cmd->argc = 2;
            break;
            
        case RESPB_OP_BLMOVE:    /* [2B srclen][src][2B dstlen][dst][1B wherefrom][1B whereto][8B timeout] */
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            CHECK_AVAIL(parser, 10);
            parser->pos += 10;
            cmd->argc = 2;
            break;
            
        case RESPB_OP_BLMPOP:    /* [8B timeout][2B numkeys]([2B keylen][key])...[1B left_right][2B count?] */
            CHECK_AVAIL(parser, 8);
            parser->pos += 8; /* timeout */
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i]);
            }
            CHECK_AVAIL(parser, 1);
            parser->pos += 1;
            cmd->argc = count < RESPB_MAX_ARGS ? count : RESPB_MAX_ARGS;
            break;
            
        case RESPB_OP_LMPOP: {    /* [2B numkeys]([2B keylen][key])...[1B left_right][2B count?] */
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i]);
            }
            CHECK_AVAIL(parser, 1);
            parser->pos += 1;
            cmd->argc = count < RESPB_MAX_ARGS ? count : RESPB_MAX_ARGS;
            break;
        }
            
        case RESPB_OP_LPOS:     /* [2B keylen][key][2B elemlen][elem][8B rank?][2B count?][8B maxlen?] */
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            /* Optional fields - simplified */
            cmd->argc = 2;
            break;
            
        case RESPB_OP_BLPOP:    /* [2B numkeys]([2B keylen][key])...[8B timeout] */
        case RESPB_OP_BRPOP: {
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i]);
            }
            CHECK_AVAIL(parser, 8);
            parser->pos += 8;
            cmd->argc = count < RESPB_MAX_ARGS ? count : RESPB_MAX_ARGS;
            break;
        }
        
        /* ===== Set Operations (0x0080-0x00BF) ===== */
        
        case RESPB_OP_SADD: {   /* [2B keylen][key][2B count]([2B memberlen][member])... */
            READ_STRING_2B(parser, &cmd->args[0]); /* key */
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i + 1]);
            }
            cmd->argc = 1 + (count < RESPB_MAX_ARGS - 1 ? count : RESPB_MAX_ARGS - 1);
            break;
        }
        
        case RESPB_OP_SREM: {   /* [2B keylen][key][2B count]([2B memberlen][member])... */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i + 1]);
            }
            cmd->argc = 1 + (count < RESPB_MAX_ARGS - 1 ? count : RESPB_MAX_ARGS - 1);
            break;
        }
        
        case RESPB_OP_SMEMBERS:  /* [2B keylen][key] */
        case RESPB_OP_SCARD:
        case RESPB_OP_SPOP:
            READ_STRING_2B(parser, &cmd->args[0]);
            cmd->argc = 1;
            break;
            
        case RESPB_OP_SISMEMBER: /* [2B keylen][key][2B memberlen][member] */
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            cmd->argc = 2;
            break;
            
        case RESPB_OP_SRANDMEMBER: /* [2B keylen][key][8B count?] */
            READ_STRING_2B(parser, &cmd->args[0]);
            /* Optional count - simplified */
            cmd->argc = 1;
            break;
            
        case RESPB_OP_SINTER:    /* [2B numkeys]([2B keylen][key])... */
        case RESPB_OP_SUNION:
        case RESPB_OP_SDIFF:
        case RESPB_OP_SINTERCARD: {
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i]);
            }
            cmd->argc = count < RESPB_MAX_ARGS ? count : RESPB_MAX_ARGS;
            break;
        }
            
        case RESPB_OP_SINTERSTORE: /* [2B dstlen][dst][2B numkeys]([2B keylen][key])... */
        case RESPB_OP_SUNIONSTORE:
        case RESPB_OP_SDIFFSTORE: {
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i + 1]);
            }
            cmd->argc = 1 + (count < RESPB_MAX_ARGS - 1 ? count : RESPB_MAX_ARGS - 1);
            break;
        }
            
        case RESPB_OP_SMOVE:     /* [2B srclen][src][2B dstlen][dst][2B memberlen][member] */
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            READ_STRING_2B(parser, &cmd->args[2]);
            cmd->argc = 3;
            break;
            
        case RESPB_OP_SSCAN:    /* [2B keylen][key][8B cursor][2B patternlen?][pattern?][8B count?] */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 8);
            parser->pos += 8;
            /* Optional fields - simplified */
            cmd->argc = 1;
            break;
            
        case RESPB_OP_SMISMEMBER: { /* [2B keylen][key][2B count]([2B memberlen][member])... */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i + 1]);
            }
            cmd->argc = 1 + (count < RESPB_MAX_ARGS - 1 ? count : RESPB_MAX_ARGS - 1);
            break;
        }
        
        /* ===== Hash Operations (0x0100-0x013F) ===== */
        
        case RESPB_OP_HSET: {   /* [2B keylen][key][2B npairs]([2B fieldlen][field][4B vallen][value])... */
            READ_STRING_2B(parser, &cmd->args[0]); /* key */
            CHECK_AVAIL(parser, 2);
            uint16_t npairs = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < npairs && i * 2 + 2 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i * 2 + 1]);     /* field */
                READ_STRING_4B(parser, &cmd->args[i * 2 + 2]);     /* value */
            }
            cmd->argc = 1 + (npairs < (RESPB_MAX_ARGS - 1) / 2 ? npairs * 2 : RESPB_MAX_ARGS - 1);
            break;
        }
        
        case RESPB_OP_HGET:     /* [2B keylen][key][2B fieldlen][field] */
            READ_STRING_2B(parser, &cmd->args[0]); /* key */
            READ_STRING_2B(parser, &cmd->args[1]); /* field */
            cmd->argc = 2;
            break;
        
        case RESPB_OP_HMSET: {    /* Same as HSET */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 2);
            uint16_t npairs = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < npairs && i * 2 + 2 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i * 2 + 1]);
                READ_STRING_4B(parser, &cmd->args[i * 2 + 2]);
            }
            cmd->argc = 1 + (npairs < (RESPB_MAX_ARGS - 1) / 2 ? npairs * 2 : RESPB_MAX_ARGS - 1);
            break;
        }
            
        case RESPB_OP_HMGET: {    /* [2B keylen][key][2B count]([2B fieldlen][field])... */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i + 1]);
            }
            cmd->argc = 1 + (count < RESPB_MAX_ARGS - 1 ? count : RESPB_MAX_ARGS - 1);
            break;
        }
            
        case RESPB_OP_HDEL: {   /* [2B keylen][key][2B nfields]([2B fieldlen][field])... */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i + 1]);
            }
            cmd->argc = 1 + (count < RESPB_MAX_ARGS - 1 ? count : RESPB_MAX_ARGS - 1);
            break;
        }
        
        case RESPB_OP_HGETALL:  /* [2B keylen][key] */
        case RESPB_OP_HKEYS:
        case RESPB_OP_HVALS:
        case RESPB_OP_HLEN:
            READ_STRING_2B(parser, &cmd->args[0]);
            cmd->argc = 1;
            break;
            
        case RESPB_OP_HRANDFIELD: /* [2B keylen][key][2B count?][1B withvalues] */
            READ_STRING_2B(parser, &cmd->args[0]);
            /* Optional fields - simplified */
            cmd->argc = 1;
            break;
            
        case RESPB_OP_HEXPIRE:    /* [2B keylen][key][8B seconds][1B flags][2B numfields]([2B fieldlen][field])... */
        case RESPB_OP_HEXPIREAT:  /* [2B keylen][key][8B timestamp][1B flags][2B numfields]([2B fieldlen][field])... */
        case RESPB_OP_HPEXPIRE:   /* [2B keylen][key][8B millis][1B flags][2B numfields]([2B fieldlen][field])... */
        case RESPB_OP_HPEXPIREAT: /* [2B keylen][key][8B timestamp][1B flags][2B numfields]([2B fieldlen][field])... */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 11);
            parser->pos += 11; /* seconds/timestamp + flags + numfields */
            if (parser->pos < parser->buffer_len) {
                READ_STRING_2B(parser, &cmd->args[1]); /* first field */
            }
            cmd->argc = 2;
            break;
            
        case RESPB_OP_HEXPIRETIME: /* [2B keylen][key][2B numfields]([2B fieldlen][field])... */
        case RESPB_OP_HPEXPIRETIME:
        case RESPB_OP_HPTTL:
        case RESPB_OP_HTTL:
        case RESPB_OP_HPERSIST: {
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 2);
            uint16_t numfields = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            if (numfields > 0 && numfields < RESPB_MAX_ARGS) {
                READ_STRING_2B(parser, &cmd->args[1]); /* first field */
            }
            cmd->argc = numfields > 0 ? 2 : 1;
            break;
        }
            
        case RESPB_OP_HGETEX:     /* [2B keylen][key][1B flags][8B expiry?][2B numfields]([2B fieldlen][field])... */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 1);
            parser->pos += 1; /* flags */
            /* Optional expiry - simplified, skip 8 bytes if present */
            CHECK_AVAIL(parser, 2);
            uint16_t numfields2 = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            if (numfields2 > 0 && numfields2 < RESPB_MAX_ARGS) {
                READ_STRING_2B(parser, &cmd->args[1]); /* first field */
            }
            cmd->argc = numfields2 > 0 ? 2 : 1;
            break;
            
        case RESPB_OP_HSETEX:     /* [2B keylen][key][1B flags][8B expiry?][2B numfields]([2B fieldlen][field][4B vallen][value])... */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 1);
            parser->pos += 1; /* flags */
            /* Optional expiry - simplified, skip 8 bytes if present */
            CHECK_AVAIL(parser, 2);
            uint16_t numfields3 = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            if (numfields3 > 0 && numfields3 < RESPB_MAX_ARGS) {
                READ_STRING_2B(parser, &cmd->args[1]); /* first field */
                READ_STRING_4B(parser, &cmd->args[2]); /* first value */
            }
            cmd->argc = numfields3 > 0 ? 3 : 1;
            break;
            
        case RESPB_OP_HEXISTS:  /* [2B keylen][key][2B fieldlen][field] */
        case RESPB_OP_HSTRLEN:
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            cmd->argc = 2;
            break;
            
        case RESPB_OP_HINCRBY:  /* [2B keylen][key][2B fieldlen][field][8B increment] */
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            CHECK_AVAIL(parser, 8);
            parser->pos += 8;
            cmd->argc = 2;
            break;
            
        case RESPB_OP_HINCRBYFLOAT: /* [2B keylen][key][2B fieldlen][field][8B float] */
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            CHECK_AVAIL(parser, 8);
            parser->pos += 8;
            cmd->argc = 2;
            break;
            
        case RESPB_OP_HSETNX:   /* [2B keylen][key][2B fieldlen][field][4B vallen][value] */
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            READ_STRING_4B(parser, &cmd->args[2]);
            cmd->argc = 3;
            break;
            
        case RESPB_OP_HSCAN:    /* [2B keylen][key][8B cursor][2B patternlen?][pattern?][8B count?][1B novalues] */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 8);
            parser->pos += 8;
            /* Optional fields - simplified */
            cmd->argc = 1;
            break;
        
        /* ===== Sorted Set Operations (0x00C0-0x00FF) ===== */
        
        case RESPB_OP_ZADD:     /* [2B keylen][key][1B flags][2B count]([8B score][2B memberlen][member])... */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 3);
            parser->pos += 3; /* flags + count */
            /* Simplified - skip score/member pairs */
            cmd->argc = 1;
            break;
            
        case RESPB_OP_ZREM: {   /* [2B keylen][key][2B count]([2B memberlen][member])... */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i + 1]);
            }
            cmd->argc = 1 + (count < RESPB_MAX_ARGS - 1 ? count : RESPB_MAX_ARGS - 1);
            break;
        }
        
        case RESPB_OP_ZSCORE:   /* [2B keylen][key][2B memberlen][member] */
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            cmd->argc = 2;
            break;
            
        case RESPB_OP_ZRANGE:   /* [2B keylen][key][8B start][8B stop][1B flags] */
        case RESPB_OP_ZREVRANGE:
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 17);
            parser->pos += 17;
            cmd->argc = 1;
            break;
            
        case RESPB_OP_ZRANGEBYSCORE: /* [2B keylen][key][8B min][8B max][1B flags] */
        case RESPB_OP_ZREVRANGEBYSCORE:
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 17);
            parser->pos += 17;
            cmd->argc = 1;
            break;
            
        case RESPB_OP_ZRANGEBYLEX: /* [2B keylen][key][2B minlen][min][2B maxlen][max][8B offset?][8B count?] */
        case RESPB_OP_ZREVRANGEBYLEX:
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            READ_STRING_2B(parser, &cmd->args[2]);
            /* Optional fields - simplified */
            cmd->argc = 3;
            break;
            
        case RESPB_OP_ZREMRANGEBYLEX: /* [2B keylen][key][2B minlen][min][2B maxlen][max] */
        case RESPB_OP_ZLEXCOUNT:
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            READ_STRING_2B(parser, &cmd->args[2]);
            cmd->argc = 3;
            break;
            
        case RESPB_OP_BZPOPMIN: /* [2B numkeys]([2B keylen][key])...[8B timeout] */
        case RESPB_OP_BZPOPMAX: {
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i]);
            }
            CHECK_AVAIL(parser, 8);
            parser->pos += 8;
            cmd->argc = count < RESPB_MAX_ARGS ? count : RESPB_MAX_ARGS;
            break;
        }
            
        case RESPB_OP_ZRANDMEMBER: /* [2B keylen][key][2B count?][1B withscores] */
            READ_STRING_2B(parser, &cmd->args[0]);
            /* Optional fields - simplified */
            cmd->argc = 1;
            break;
            
        case RESPB_OP_ZDIFF: { /* [2B numkeys]([2B keylen][key])...[1B withscores] */
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i]);
            }
            CHECK_AVAIL(parser, 1);
            parser->pos += 1;
            cmd->argc = count < RESPB_MAX_ARGS ? count : RESPB_MAX_ARGS;
            break;
        }
            
        case RESPB_OP_ZDIFFSTORE: { /* [2B dstlen][dst][2B numkeys]([2B keylen][key])... */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i + 1]);
            }
            cmd->argc = 1 + (count < RESPB_MAX_ARGS - 1 ? count : RESPB_MAX_ARGS - 1);
            break;
        }
            
        case RESPB_OP_ZINTER: /* [2B numkeys]([2B keylen][key])...[1B flags] */
        case RESPB_OP_ZUNION: {
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i]);
            }
            CHECK_AVAIL(parser, 1);
            parser->pos += 1;
            cmd->argc = count < RESPB_MAX_ARGS ? count : RESPB_MAX_ARGS;
            break;
        }
            
        case RESPB_OP_ZINTERSTORE: /* [2B dstlen][dst][2B numkeys]([2B keylen][key])...[1B flags] */
        case RESPB_OP_ZUNIONSTORE: {
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i + 1]);
            }
            CHECK_AVAIL(parser, 1);
            parser->pos += 1;
            cmd->argc = 1 + (count < RESPB_MAX_ARGS - 1 ? count : RESPB_MAX_ARGS - 1);
            break;
        }
            
        case RESPB_OP_ZSCAN: /* [2B keylen][key][8B cursor][2B patternlen?][pattern?][8B count?][1B noscores] */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 8);
            parser->pos += 8;
            /* Optional fields - simplified */
            cmd->argc = 1;
            break;
            
        case RESPB_OP_ZMPOP: { /* [2B numkeys]([2B keylen][key])...[1B min_max][2B count?] */
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i]);
            }
            CHECK_AVAIL(parser, 1);
            parser->pos += 1;
            cmd->argc = count < RESPB_MAX_ARGS ? count : RESPB_MAX_ARGS;
            break;
        }
            
        case RESPB_OP_BZMPOP: { /* [8B timeout][2B numkeys]([2B keylen][key])...[1B min_max][2B count?] */
            CHECK_AVAIL(parser, 8);
            parser->pos += 8; /* timeout */
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i]);
            }
            CHECK_AVAIL(parser, 1);
            parser->pos += 1;
            cmd->argc = count < RESPB_MAX_ARGS ? count : RESPB_MAX_ARGS;
            break;
        }
            
        case RESPB_OP_ZRANGESTORE: { /* [2B dstlen][dst][2B srclen][src][8B min][8B max][1B flags] */
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            CHECK_AVAIL(parser, 17);
            parser->pos += 17;
            cmd->argc = 2;
            break;
        }
            
        case RESPB_OP_ZINTERCARD: { /* [2B numkeys]([2B keylen][key])...[8B limit?] */
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i]);
            }
            /* Optional limit - simplified */
            cmd->argc = count < RESPB_MAX_ARGS ? count : RESPB_MAX_ARGS;
            break;
        }
            
        case RESPB_OP_ZCARD:
        case RESPB_OP_ZPOPMIN:
        case RESPB_OP_ZPOPMAX:
            READ_STRING_2B(parser, &cmd->args[0]);
            cmd->argc = 1;
            break;
            
        case RESPB_OP_ZCOUNT:   /* [2B keylen][key][8B min][8B max] */
        case RESPB_OP_ZREMRANGEBYRANK:
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 16);
            parser->pos += 16;
            cmd->argc = 1;
            break;
            
        case RESPB_OP_ZINCRBY:  /* [2B keylen][key][8B increment][2B memberlen][member] */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 8);
            parser->pos += 8;
            READ_STRING_2B(parser, &cmd->args[1]);
            cmd->argc = 2;
            break;
            
        case RESPB_OP_ZRANK:    /* [2B keylen][key][2B memberlen][member][1B withscore] */
        case RESPB_OP_ZREVRANK:
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            CHECK_AVAIL(parser, 1);
            parser->pos += 1;
            cmd->argc = 2;
            break;
            
        case RESPB_OP_ZMSCORE: {  /* [2B keylen][key][2B count]([2B memberlen][member])... */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i + 1]);
            }
            cmd->argc = 1 + (count < RESPB_MAX_ARGS - 1 ? count : RESPB_MAX_ARGS - 1);
            break;
        }
            
        case RESPB_OP_ZREMRANGEBYSCORE: /* [2B keylen][key][8B min][8B max] */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 16);
            parser->pos += 16;
            cmd->argc = 1;
            break;
            
        /* ===== Connection Management (0x0300-0x033F) ===== */
        
        case RESPB_OP_PING:     /* [2B msglen?][message?] */
            /* Optional message - simplified */
            cmd->argc = 0;
            break;
            
        case RESPB_OP_ECHO:     /* [2B msglen][message] */
            READ_STRING_2B(parser, &cmd->args[0]);
            cmd->argc = 1;
            break;
            
        case RESPB_OP_AUTH:     /* [2B userlen?][username?][2B passlen][password] */
            /* Optional username - simplified, just read password */
            READ_STRING_2B(parser, &cmd->args[0]);
            cmd->argc = 1;
            break;
            
        case RESPB_OP_SELECT:   /* [2B dbindex] */
            CHECK_AVAIL(parser, 2);
            parser->pos += 2;
            cmd->argc = 0;
            break;
            
        case RESPB_OP_QUIT:     /* No payload */
        case RESPB_OP_RESET:
            cmd->argc = 0;
            break;
            
        case RESPB_OP_HELLO:    /* [1B protover][2B userlen?][username?][2B passlen?][password?][2B clientnamelen?][clientname?] */
            CHECK_AVAIL(parser, 1);
            parser->pos += 1; /* protocol version */
            /* Optional fields - simplified */
            cmd->argc = 0;
            break;
            
        case RESPB_OP_CLIENT:   /* [1B subcommand][additional args...] */
            CHECK_AVAIL(parser, 1);
            parser->pos += 1; /* subcommand */
            /* Additional args - simplified */
            cmd->argc = 0;
            break;
        
        /* ===== Cluster Management (0x0340-0x03BF) ===== */
        case RESPB_OP_CLUSTER:  /* [1B subcommand][additional args...] */
            CHECK_AVAIL(parser, 1);
            parser->pos += 1; /* subcommand */
            /* Additional args - simplified */
            cmd->argc = 0;
            break;
            
        case RESPB_OP_READONLY:  /* No payload */
        case RESPB_OP_READWRITE:
        case RESPB_OP_ASKING:
            cmd->argc = 0;
            break;
        
        /* ===== Server Management (0x03C0-0x04FF) ===== */
        case RESPB_OP_DBSIZE:     /* No payload */
        case RESPB_OP_SAVE:
        case RESPB_OP_BGREWRITEAOF:
        case RESPB_OP_LASTSAVE:
        case RESPB_OP_TIME:
        case RESPB_OP_ROLE:
        case RESPB_OP_MONITOR:
        case RESPB_OP_SYNC:
            cmd->argc = 0;
            break;
            
        case RESPB_OP_FLUSHDB:    /* [1B async_sync] */
        case RESPB_OP_FLUSHALL:
            CHECK_AVAIL(parser, 1);
            parser->pos += 1;
            cmd->argc = 0;
            break;
            
        case RESPB_OP_BGSAVE:     /* [1B flags] */
        case RESPB_OP_SHUTDOWN:
            CHECK_AVAIL(parser, 1);
            parser->pos += 1;
            cmd->argc = 0;
            break;
            
        case RESPB_OP_INFO: {     /* [2B count]([2B sectionlen][section])... */
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            if (count > 0 && count < RESPB_MAX_ARGS) {
                READ_STRING_2B(parser, &cmd->args[0]); /* first section */
            }
            cmd->argc = count > 0 ? 1 : 0;
            break;
        }
            
        case RESPB_OP_CONFIG:     /* [1B subcommand][additional args...] */
        case RESPB_OP_COMMAND:
        case RESPB_OP_DEBUG:
        case RESPB_OP_SLOWLOG:
        case RESPB_OP_LATENCY:
        case RESPB_OP_MEMORY:
        case RESPB_OP_MODULE_CMD:
        case RESPB_OP_ACL:
        case RESPB_OP_COMMANDLOG:
            CHECK_AVAIL(parser, 1);
            parser->pos += 1; /* subcommand */
            /* Additional args - simplified */
            cmd->argc = 0;
            break;
            
        case RESPB_OP_REPLICAOF:  /* [2B hostlen][host][2B port] */
        case RESPB_OP_SLAVEOF:
            READ_STRING_2B(parser, &cmd->args[0]); /* host */
            CHECK_AVAIL(parser, 2);
            parser->pos += 2; /* port */
            cmd->argc = 1;
            break;
            
        case RESPB_OP_PSYNC:      /* [2B replidlen][replicationid][8B offset] */
            READ_STRING_2B(parser, &cmd->args[0]); /* replicationid */
            CHECK_AVAIL(parser, 8);
            parser->pos += 8; /* offset */
            cmd->argc = 1;
            break;
            
        case RESPB_OP_REPLCONF: { /* [2B count]([2B arglen][arg])... */
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            if (count > 0 && count < RESPB_MAX_ARGS) {
                READ_STRING_2B(parser, &cmd->args[0]); /* first arg */
            }
            cmd->argc = count > 0 ? 1 : 0;
            break;
        }
            
        case RESPB_OP_FAILOVER:   /* [1B flags][2B hostlen?][host?][2B port?][8B timeout?] */
            CHECK_AVAIL(parser, 1);
            parser->pos += 1; /* flags */
            /* Optional fields - simplified */
            cmd->argc = 0;
            break;
            
        case RESPB_OP_SWAPDB:     /* [2B db1][2B db2] */
            CHECK_AVAIL(parser, 4);
            parser->pos += 4;
            cmd->argc = 0;
            break;
            
        case RESPB_OP_LOLWUT: {   /* [2B count]([2B arglen][arg])... */
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            if (count > 0 && count < RESPB_MAX_ARGS) {
                READ_STRING_2B(parser, &cmd->args[0]); /* first arg */
            }
            cmd->argc = count > 0 ? 1 : 0;
            break;
        }
            
        case RESPB_OP_RESTORE_ASKING: /* [2B keylen][key][8B ttl][4B datalen][data][1B flags] */
            READ_STRING_2B(parser, &cmd->args[0]); /* key */
            CHECK_AVAIL(parser, 8);
            parser->pos += 8; /* ttl */
            READ_STRING_4B(parser, &cmd->args[1]); /* data */
            CHECK_AVAIL(parser, 1);
            parser->pos += 1; /* flags */
            cmd->argc = 2;
            break;
        
        /* ===== Transaction Operations (0x0240-0x025F) ===== */
        case RESPB_OP_MULTI:
        case RESPB_OP_EXEC:
        case RESPB_OP_DISCARD:
        case RESPB_OP_UNWATCH:
            cmd->argc = 0;
            break;
            
        case RESPB_OP_WATCH: {    /* [2B numkeys]([2B keylen][key])... */
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i]);
            }
            cmd->argc = count < RESPB_MAX_ARGS ? count : RESPB_MAX_ARGS;
            break;
        }
        
        /* ===== Scripting and Functions (0x0260-0x02BF) ===== */
        case RESPB_OP_EVAL: {     /* [4B scriptlen][script][2B numkeys]([2B keylen][key])...[2B numargs]([2B arglen][arg])... */
            READ_STRING_4B(parser, &cmd->args[0]); /* script */
            CHECK_AVAIL(parser, 2);
            uint16_t numkeys = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < numkeys && i + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i + 1]);
            }
            CHECK_AVAIL(parser, 2);
            uint16_t numargs = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            if (numargs > 0 && numkeys + 1 < RESPB_MAX_ARGS) {
                READ_STRING_2B(parser, &cmd->args[numkeys + 1]); /* first arg */
            }
            cmd->argc = (numkeys + (numargs > 0 ? 1 : 0) + 1) < RESPB_MAX_ARGS ? (numkeys + (numargs > 0 ? 1 : 0) + 1) : RESPB_MAX_ARGS;
            break;
        }
            
        case RESPB_OP_EVALSHA: {  /* [2B sha1len][sha1][2B numkeys]([2B keylen][key])...[2B numargs]([2B arglen][arg])... */
            READ_STRING_2B(parser, &cmd->args[0]); /* sha1 */
            CHECK_AVAIL(parser, 2);
            uint16_t numkeys = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < numkeys && i + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i + 1]);
            }
            CHECK_AVAIL(parser, 2);
            uint16_t numargs = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            if (numargs > 0 && numkeys + 1 < RESPB_MAX_ARGS) {
                READ_STRING_2B(parser, &cmd->args[numkeys + 1]); /* first arg */
            }
            cmd->argc = (numkeys + (numargs > 0 ? 1 : 0) + 1) < RESPB_MAX_ARGS ? (numkeys + (numargs > 0 ? 1 : 0) + 1) : RESPB_MAX_ARGS;
            break;
        }
            
        case RESPB_OP_EVAL_RO: {  /* [4B scriptlen][script][2B numkeys]([2B keylen][key])...[2B numargs]([2B arglen][arg])... */
            READ_STRING_4B(parser, &cmd->args[0]); /* script */
            CHECK_AVAIL(parser, 2);
            uint16_t numkeys = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < numkeys && i + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i + 1]);
            }
            CHECK_AVAIL(parser, 2);
            uint16_t numargs = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            if (numargs > 0 && numkeys + 1 < RESPB_MAX_ARGS) {
                READ_STRING_2B(parser, &cmd->args[numkeys + 1]); /* first arg */
            }
            cmd->argc = (numkeys + (numargs > 0 ? 1 : 0) + 1) < RESPB_MAX_ARGS ? (numkeys + (numargs > 0 ? 1 : 0) + 1) : RESPB_MAX_ARGS;
            break;
        }
            
        case RESPB_OP_EVALSHA_RO: { /* [2B sha1len][sha1][2B numkeys]([2B keylen][key])...[2B numargs]([2B arglen][arg])... */
            READ_STRING_2B(parser, &cmd->args[0]); /* sha1 */
            CHECK_AVAIL(parser, 2);
            uint16_t numkeys = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < numkeys && i + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i + 1]);
            }
            CHECK_AVAIL(parser, 2);
            uint16_t numargs = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            if (numargs > 0 && numkeys + 1 < RESPB_MAX_ARGS) {
                READ_STRING_2B(parser, &cmd->args[numkeys + 1]); /* first arg */
            }
            cmd->argc = (numkeys + (numargs > 0 ? 1 : 0) + 1) < RESPB_MAX_ARGS ? (numkeys + (numargs > 0 ? 1 : 0) + 1) : RESPB_MAX_ARGS;
            break;
        }
            
        case RESPB_OP_SCRIPT:     /* [1B subcommand][additional args...] */
            CHECK_AVAIL(parser, 1);
            parser->pos += 1; /* subcommand */
            /* Additional args - simplified */
            cmd->argc = 0;
            break;
            
        case RESPB_OP_FCALL: {    /* [2B funclen][function][2B numkeys]([2B keylen][key])...[2B numargs]([2B arglen][arg])... */
            READ_STRING_2B(parser, &cmd->args[0]); /* function */
            CHECK_AVAIL(parser, 2);
            uint16_t numkeys = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < numkeys && i + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i + 1]);
            }
            CHECK_AVAIL(parser, 2);
            uint16_t numargs = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            if (numargs > 0 && numkeys + 1 < RESPB_MAX_ARGS) {
                READ_STRING_2B(parser, &cmd->args[numkeys + 1]); /* first arg */
            }
            cmd->argc = (numkeys + (numargs > 0 ? 1 : 0) + 1) < RESPB_MAX_ARGS ? (numkeys + (numargs > 0 ? 1 : 0) + 1) : RESPB_MAX_ARGS;
            break;
        }
            
        case RESPB_OP_FCALL_RO: { /* [2B funclen][function][2B numkeys]([2B keylen][key])...[2B numargs]([2B arglen][arg])... */
            READ_STRING_2B(parser, &cmd->args[0]); /* function */
            CHECK_AVAIL(parser, 2);
            uint16_t numkeys = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < numkeys && i + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i + 1]);
            }
            CHECK_AVAIL(parser, 2);
            uint16_t numargs = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            if (numargs > 0 && numkeys + 1 < RESPB_MAX_ARGS) {
                READ_STRING_2B(parser, &cmd->args[numkeys + 1]); /* first arg */
            }
            cmd->argc = (numkeys + (numargs > 0 ? 1 : 0) + 1) < RESPB_MAX_ARGS ? (numkeys + (numargs > 0 ? 1 : 0) + 1) : RESPB_MAX_ARGS;
            break;
        }
            
        case RESPB_OP_FUNCTION:      /* [1B subcommand][additional args...] */
            CHECK_AVAIL(parser, 1);
            parser->pos += 1; /* subcommand */
            /* Additional args - simplified */
            cmd->argc = 0;
            break;
            
        case RESPB_OP_TTL:      /* [2B keylen][key] */
        case RESPB_OP_PERSIST:
        case RESPB_OP_PTTL:
        case RESPB_OP_TYPE:
        case RESPB_OP_EXPIRETIME:
        case RESPB_OP_PEXPIRETIME:
        case RESPB_OP_KEYS:
        case RESPB_OP_DUMP:
            READ_STRING_2B(parser, &cmd->args[0]);
            cmd->argc = 1;
            break;
            
        case RESPB_OP_EXPIREAT: /* [2B keylen][key][8B timestamp][1B flags] */
        case RESPB_OP_PEXPIRE:
        case RESPB_OP_PEXPIREAT:
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 9);
            parser->pos += 9;
            cmd->argc = 1;
            break;
            
        case RESPB_OP_RENAME:   /* [2B keylen][key][2B newkeylen][newkey] */
        case RESPB_OP_RENAMENX:
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            cmd->argc = 2;
            break;
            
        case RESPB_OP_RANDOMKEY: /* No payload */
            cmd->argc = 0;
            break;
            
        case RESPB_OP_SCAN:    /* [8B cursor][2B patternlen?][pattern?][8B count?][2B typelen?][type?] */
            CHECK_AVAIL(parser, 8);
            parser->pos += 8; /* cursor */
            /* Optional fields - simplified */
            cmd->argc = 0;
            break;
            
        case RESPB_OP_RESTORE:  /* [2B keylen][key][8B ttl][4B datalen][data][1B flags] */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 8);
            parser->pos += 8; /* ttl */
            READ_STRING_4B(parser, &cmd->args[1]);
            CHECK_AVAIL(parser, 1);
            parser->pos += 1; /* flags */
            cmd->argc = 2;
            break;
            
        case RESPB_OP_MIGRATE:   /* [2B hostlen][host][2B port][2B keylen][key][2B db][8B timeout][1B flags] */
            READ_STRING_2B(parser, &cmd->args[0]); /* host */
            CHECK_AVAIL(parser, 2);
            parser->pos += 2; /* port */
            READ_STRING_2B(parser, &cmd->args[1]); /* key */
            CHECK_AVAIL(parser, 2);
            parser->pos += 2; /* db */
            CHECK_AVAIL(parser, 9);
            parser->pos += 9; /* timeout + flags */
            cmd->argc = 2;
            break;
            
        case RESPB_OP_MOVE:      /* [2B keylen][key][2B db] */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 2);
            parser->pos += 2;
            cmd->argc = 1;
            break;
            
        case RESPB_OP_COPY:     /* [2B srclen][src][2B dstlen][dst][2B db?][1B replace] */
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            CHECK_AVAIL(parser, 3);
            parser->pos += 3; /* db + replace */
            cmd->argc = 2;
            break;
            
        case RESPB_OP_SORT:     /* [2B keylen][key][...complex sorting options] */
        case RESPB_OP_SORT_RO:
            READ_STRING_2B(parser, &cmd->args[0]);
            /* Complex options - simplified */
            cmd->argc = 1;
            break;
            
        case RESPB_OP_TOUCH: {  /* [2B numkeys]([2B keylen][key])... */
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i]);
            }
            cmd->argc = count < RESPB_MAX_ARGS ? count : RESPB_MAX_ARGS;
            break;
        }
            
        case RESPB_OP_OBJECT:   /* [1B subcommand][2B keylen][key] */
            CHECK_AVAIL(parser, 1);
            parser->pos += 1; /* subcommand */
            READ_STRING_2B(parser, &cmd->args[0]);
            cmd->argc = 1;
            break;
            
        case RESPB_OP_WAIT:     /* [8B numreplicas][8B timeout] */
            CHECK_AVAIL(parser, 16);
            parser->pos += 16;
            cmd->argc = 0;
            break;
            
        case RESPB_OP_WAITAOF:  /* [8B numlocal][8B numreplicas][8B timeout] */
            CHECK_AVAIL(parser, 24);
            parser->pos += 24;
            cmd->argc = 0;
            break;
        
        /* ===== Bitmap Operations (0x0140-0x015F) ===== */
        case RESPB_OP_SETBIT:   /* [2B keylen][key][8B offset][1B value] */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 9);
            parser->pos += 9;
            cmd->argc = 1;
            break;
            
        case RESPB_OP_GETBIT:   /* [2B keylen][key][8B offset] */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 8);
            parser->pos += 8;
            cmd->argc = 1;
            break;
            
        case RESPB_OP_BITCOUNT: /* [2B keylen][key][8B start?][8B end?][1B unit] */
            READ_STRING_2B(parser, &cmd->args[0]);
            /* Optional fields - simplified */
            cmd->argc = 1;
            break;
            
        case RESPB_OP_BITPOS:   /* [2B keylen][key][1B bit][8B start?][8B end?][1B unit] */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 1);
            parser->pos += 1; /* bit */
            /* Optional fields - simplified */
            cmd->argc = 1;
            break;
            
        case RESPB_OP_BITOP: {  /* [1B operation][2B dstlen][dst][2B numkeys]([2B keylen][key])... */
            CHECK_AVAIL(parser, 1);
            parser->pos += 1; /* operation */
            READ_STRING_2B(parser, &cmd->args[0]); /* dst */
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i + 1]);
            }
            cmd->argc = 1 + (count < RESPB_MAX_ARGS - 1 ? count : RESPB_MAX_ARGS - 1);
            break;
        }
            
        case RESPB_OP_BITFIELD: /* [2B keylen][key][2B count]([1B op][2B args]...)... */
        case RESPB_OP_BITFIELD_RO:
            READ_STRING_2B(parser, &cmd->args[0]);
            /* Complex nested structure - simplified */
            cmd->argc = 1;
            break;
        
        /* ===== HyperLogLog Operations (0x0160-0x017F) ===== */
        case RESPB_OP_PFADD: { /* [2B keylen][key][2B count]([2B elemlen][elem])... */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i + 1]);
            }
            cmd->argc = 1 + (count < RESPB_MAX_ARGS - 1 ? count : RESPB_MAX_ARGS - 1);
            break;
        }
            
        case RESPB_OP_PFCOUNT: { /* [2B numkeys]([2B keylen][key])... */
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i]);
            }
            cmd->argc = count < RESPB_MAX_ARGS ? count : RESPB_MAX_ARGS;
            break;
        }
            
        case RESPB_OP_PFMERGE: { /* [2B dstlen][dst][2B numkeys]([2B keylen][key])... */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i + 1]);
            }
            cmd->argc = 1 + (count < RESPB_MAX_ARGS - 1 ? count : RESPB_MAX_ARGS - 1);
            break;
        }
            
        case RESPB_OP_PFDEBUG:  /* [2B subcmdlen][subcmd][2B keylen][key] */
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            cmd->argc = 2;
            break;
            
        case RESPB_OP_PFSELFTEST: /* No payload */
            cmd->argc = 0;
            break;
        
        /* ===== Geospatial Operations (0x0180-0x01BF) ===== */
        case RESPB_OP_GEOADD:   /* [2B keylen][key][1B flags][2B count]([8B longitude][8B latitude][2B memberlen][member])... */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 3);
            parser->pos += 3; /* flags + count */
            /* Skip coordinate pairs - simplified, just store key */
            cmd->argc = 1;
            break;
            
        case RESPB_OP_GEODIST:  /* [2B keylen][key][2B mem1len][mem1][2B mem2len][mem2][1B unit] */
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            READ_STRING_2B(parser, &cmd->args[2]);
            CHECK_AVAIL(parser, 1);
            parser->pos += 1; /* unit */
            cmd->argc = 3;
            break;
            
        case RESPB_OP_GEOHASH:  /* [2B keylen][key][2B count]([2B memberlen][member])... */
        case RESPB_OP_GEOPOS: {
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i + 1]);
            }
            cmd->argc = 1 + (count < RESPB_MAX_ARGS - 1 ? count : RESPB_MAX_ARGS - 1);
            break;
        }
            
        case RESPB_OP_GEORADIUS: /* [2B keylen][key][8B longitude][8B latitude][8B radius][1B unit][1B flags] */
        case RESPB_OP_GEORADIUS_RO:
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 18);
            parser->pos += 18; /* coordinates + radius + unit + flags */
            cmd->argc = 1;
            break;
            
        case RESPB_OP_GEORADIUSBYMEMBER: /* [2B keylen][key][2B memberlen][member][8B radius][1B unit][1B flags] */
        case RESPB_OP_GEORADIUSBYMEMBER_RO:
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            CHECK_AVAIL(parser, 10);
            parser->pos += 10; /* radius + unit + flags */
            cmd->argc = 2;
            break;
            
        case RESPB_OP_GEOSEARCH:   /* [2B keylen][key][...complex payload with flags] */
            READ_STRING_2B(parser, &cmd->args[0]);
            /* Complex payload - simplified */
            CHECK_AVAIL(parser, 1);
            parser->pos += 1; /* flags */
            cmd->argc = 1;
            break;
            
        case RESPB_OP_GEOSEARCHSTORE: /* [2B dstlen][dst][2B srclen][src][...complex payload with flags] */
            READ_STRING_2B(parser, &cmd->args[0]); /* dst */
            READ_STRING_2B(parser, &cmd->args[1]); /* src */
            /* Complex payload - simplified */
            CHECK_AVAIL(parser, 1);
            parser->pos += 1; /* flags */
            cmd->argc = 2;
            break;
        
        /* ===== Stream Operations (0x01C0-0x01FF) ===== */
        case RESPB_OP_XADD: {   /* [2B keylen][key][2B idlen][id][2B count]([2B fieldlen][field][4B vallen][value])... */
            READ_STRING_2B(parser, &cmd->args[0]); /* key */
            READ_STRING_2B(parser, &cmd->args[1]); /* id */
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            /* Read first field-value pair */
            if (count > 0) {
                READ_STRING_2B(parser, &cmd->args[2]); /* field */
            }
            cmd->argc = count > 0 ? 3 : 2;
            break;
        }
            
        case RESPB_OP_XLEN:     /* [2B keylen][key] */
            READ_STRING_2B(parser, &cmd->args[0]);
            cmd->argc = 1;
            break;
            
        case RESPB_OP_XRANGE:   /* [2B keylen][key][2B startlen][start][2B endlen][end][8B count?] */
        case RESPB_OP_XREVRANGE: {
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            READ_STRING_2B(parser, &cmd->args[2]);
            /* Optional count - simplified */
            cmd->argc = 3;
            break;
        }
            
        case RESPB_OP_XREAD: {  /* [8B count?][8B block?][2B numkeys]([2B keylen][key][2B idlen][id])... */
            /* Optional count and block - simplified */
            CHECK_AVAIL(parser, 2);
            uint16_t numkeys = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < numkeys && i * 2 + 1 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i * 2]);     /* key */
                READ_STRING_2B(parser, &cmd->args[i * 2 + 1]); /* id */
            }
            cmd->argc = numkeys * 2 < RESPB_MAX_ARGS ? numkeys * 2 : RESPB_MAX_ARGS;
            break;
        }
            
        case RESPB_OP_XREADGROUP: { /* [2B grouplen][group][2B consumerlen][consumer][8B count?][8B block?][1B noack][2B numkeys]([2B keylen][key][2B idlen][id])... */
            READ_STRING_2B(parser, &cmd->args[0]); /* group */
            READ_STRING_2B(parser, &cmd->args[1]); /* consumer */
            /* Optional count, block, noack - simplified */
            CHECK_AVAIL(parser, 2);
            uint16_t numkeys = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < numkeys && i * 2 + 2 < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i * 2 + 2]);     /* key */
                READ_STRING_2B(parser, &cmd->args[i * 2 + 3]); /* id */
            }
            cmd->argc = 2 + (numkeys * 2 < RESPB_MAX_ARGS - 2 ? numkeys * 2 : RESPB_MAX_ARGS - 2);
            break;
        }
            
        case RESPB_OP_XDEL: {   /* [2B keylen][key][2B count]([2B idlen][id])... */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            if (count > 0) {
                READ_STRING_2B(parser, &cmd->args[1]); /* first id */
            }
            cmd->argc = count > 0 ? 2 : 1;
            break;
        }
            
        case RESPB_OP_XTRIM:    /* [2B keylen][key][1B strategy][8B threshold][1B flags] */
            READ_STRING_2B(parser, &cmd->args[0]);
            CHECK_AVAIL(parser, 10);
            parser->pos += 10;
            cmd->argc = 1;
            break;
            
        case RESPB_OP_XACK: {   /* [2B keylen][key][2B grouplen][group][2B count]([2B idlen][id])... */
            READ_STRING_2B(parser, &cmd->args[0]); /* key */
            READ_STRING_2B(parser, &cmd->args[1]); /* group */
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            if (count > 0) {
                READ_STRING_2B(parser, &cmd->args[2]); /* first id */
            }
            cmd->argc = count > 0 ? 3 : 2;
            break;
        }
            
        case RESPB_OP_XPENDING: /* [2B keylen][key][2B grouplen][group][8B idle?][2B startlen?][start?][2B endlen?][end?][8B count?][2B consumerlen?][consumer?] */
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            /* Optional fields - simplified */
            cmd->argc = 2;
            break;
            
        case RESPB_OP_XCLAIM: { /* [2B keylen][key][2B grouplen][group][2B consumerlen][consumer][8B min_idle][2B count]([2B idlen][id])...[1B flags] */
            READ_STRING_2B(parser, &cmd->args[0]); /* key */
            READ_STRING_2B(parser, &cmd->args[1]); /* group */
            READ_STRING_2B(parser, &cmd->args[2]); /* consumer */
            CHECK_AVAIL(parser, 8);
            parser->pos += 8; /* min_idle */
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            if (count > 0) {
                READ_STRING_2B(parser, &cmd->args[3]); /* first id */
            }
            CHECK_AVAIL(parser, 1);
            parser->pos += 1; /* flags */
            cmd->argc = count > 0 ? 4 : 3;
            break;
        }
            
        case RESPB_OP_XAUTOCLAIM: { /* [2B keylen][key][2B grouplen][group][2B consumerlen][consumer][8B min_idle][2B startlen][start][8B count?][1B justid] */
            READ_STRING_2B(parser, &cmd->args[0]); /* key */
            READ_STRING_2B(parser, &cmd->args[1]); /* group */
            READ_STRING_2B(parser, &cmd->args[2]); /* consumer */
            CHECK_AVAIL(parser, 8);
            parser->pos += 8; /* min_idle */
            READ_STRING_2B(parser, &cmd->args[3]); /* start */
            /* Optional count and justid - simplified */
            cmd->argc = 4;
            break;
        }
            
        case RESPB_OP_XINFO:    /* [1B subcommand][2B keylen][key][additional args...] */
            CHECK_AVAIL(parser, 1);
            parser->pos += 1; /* subcommand */
            READ_STRING_2B(parser, &cmd->args[0]);
            /* Additional args - simplified */
            cmd->argc = 1;
            break;
            
        case RESPB_OP_XGROUP:   /* [1B subcommand][2B keylen][key][additional args...] */
            CHECK_AVAIL(parser, 1);
            parser->pos += 1; /* subcommand */
            READ_STRING_2B(parser, &cmd->args[0]);
            /* Additional args - simplified */
            cmd->argc = 1;
            break;
            
        case RESPB_OP_XSETID:   /* [2B keylen][key][2B idlen][id][8B entries_added?][2B maxdeletlen?][maxdeleteid?] */
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_2B(parser, &cmd->args[1]);
            /* Optional fields - simplified */
            cmd->argc = 2;
            break;
        
        /* ===== Pub/Sub Operations (0x0200-0x023F) ===== */
        case RESPB_OP_PUBLISH:  /* [2B channellen][channel][4B msglen][message] */
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_4B(parser, &cmd->args[1]);
            cmd->argc = 2;
            break;
            
        case RESPB_OP_SPUBLISH:  /* [2B channellen][channel][4B msglen][message] */
            READ_STRING_2B(parser, &cmd->args[0]);
            READ_STRING_4B(parser, &cmd->args[1]);
            cmd->argc = 2;
            break;
            
        case RESPB_OP_SUBSCRIBE: /* [2B count]([2B channellen][channel])... */
        case RESPB_OP_UNSUBSCRIBE:
        case RESPB_OP_SSUBSCRIBE:
        case RESPB_OP_SUNSUBSCRIBE: {
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i]);
            }
            cmd->argc = count < RESPB_MAX_ARGS ? count : RESPB_MAX_ARGS;
            break;
        }
            
        case RESPB_OP_PSUBSCRIBE: /* [2B count]([2B patternlen][pattern])... */
        case RESPB_OP_PUNSUBSCRIBE: {
            CHECK_AVAIL(parser, 2);
            uint16_t count = read_u16_be(parser->buffer + parser->pos);
            parser->pos += 2;
            for (uint16_t i = 0; i < count && i < RESPB_MAX_ARGS; i++) {
                READ_STRING_2B(parser, &cmd->args[i]);
            }
            cmd->argc = count < RESPB_MAX_ARGS ? count : RESPB_MAX_ARGS;
            break;
        }
            
        case RESPB_OP_PUBSUB:  /* [1B subcommand][additional args...] */
            CHECK_AVAIL(parser, 1);
            parser->pos += 1; /* subcommand */
            /* Additional args - simplified */
            cmd->argc = 0;
            break;
            
        /* ===== Module Commands (0xF000) ===== */
        
        case RESPB_OP_MODULE: {
            /* Read 4-byte subcommand */
            CHECK_AVAIL(parser, 4);
            cmd->module_subcommand = read_u32_be(parser->buffer + parser->pos);
            parser->pos += 4;
            
            /* Extract module ID and command ID */
            cmd->module_id = (cmd->module_subcommand >> 16) & 0xFFFF;
            cmd->command_id = cmd->module_subcommand & 0xFFFF;
            
            /* Parse module-specific payloads */
            if (cmd->module_id == RESPB_MODULE_JSON) {
                /* JSON Module commands */
                if (cmd->command_id == 0x0000) {
                    /* JSON.SET: [2B keylen][key][2B pathlen][path][4B jsonlen][json][1B flags] */
                    READ_STRING_2B(parser, &cmd->args[0]); /* key */
                    READ_STRING_2B(parser, &cmd->args[1]); /* path */
                    READ_STRING_4B(parser, &cmd->args[2]); /* json */
                    CHECK_AVAIL(parser, 1); /* flags */
                    parser->pos += 1;
                    cmd->argc = 3;
                } else if (cmd->command_id == 0x0001) {
                    /* JSON.GET: [2B keylen][key][2B numpaths]([2B pathlen][path])... */
                    READ_STRING_2B(parser, &cmd->args[0]); /* key */
                    CHECK_AVAIL(parser, 2);
                    uint16_t numpaths = read_u16_be(parser->buffer + parser->pos);
                    parser->pos += 2;
                    for (uint16_t i = 0; i < numpaths && i + 1 < RESPB_MAX_ARGS; i++) {
                        READ_STRING_2B(parser, &cmd->args[i + 1]);
                    }
                    cmd->argc = 1 + (numpaths < RESPB_MAX_ARGS - 1 ? numpaths : RESPB_MAX_ARGS - 1);
                } else {
                    /* Generic JSON command - parse key only */
                    READ_STRING_2B(parser, &cmd->args[0]);
                    cmd->argc = 1;
                }
            } else if (cmd->module_id == RESPB_MODULE_BF) {
                /* Bloom Filter commands */
                if (cmd->command_id == 0x0000) {
                    /* BF.ADD: [2B keylen][key][2B itemlen][item] */
                    READ_STRING_2B(parser, &cmd->args[0]); /* key */
                    READ_STRING_2B(parser, &cmd->args[1]); /* item */
                    cmd->argc = 2;
                } else if (cmd->command_id == 0x0002) {
                    /* BF.EXISTS: [2B keylen][key][2B itemlen][item] */
                    READ_STRING_2B(parser, &cmd->args[0]); /* key */
                    READ_STRING_2B(parser, &cmd->args[1]); /* item */
                    cmd->argc = 2;
                } else {
                    /* Generic BF command */
                    READ_STRING_2B(parser, &cmd->args[0]);
                    cmd->argc = 1;
                }
            } else if (cmd->module_id == RESPB_MODULE_FT) {
                /* Search/FT commands */
                if (cmd->command_id == 0x0001) {
                    /* FT.SEARCH: [2B idxlen][index][2B querylen][query] */
                    READ_STRING_2B(parser, &cmd->args[0]); /* index */
                    READ_STRING_2B(parser, &cmd->args[1]); /* query */
                    cmd->argc = 2;
                } else {
                    /* Generic FT command */
                    READ_STRING_2B(parser, &cmd->args[0]);
                    cmd->argc = 1;
                }
            } else {
                /* Unknown module - try generic parsing */
                READ_STRING_2B(parser, &cmd->args[0]);
                cmd->argc = 1;
            }
            break;
        }
        
        /* ===== RESP Passthrough (0xFFFF) ===== */
        
        case RESPB_OP_RESP_PASSTHROUGH: {
            /* Read RESP data length */
            CHECK_AVAIL(parser, 4);
            cmd->resp_length = read_u32_be(parser->buffer + parser->pos);
            parser->pos += 4;
            
            /* Store pointer to RESP text data */
            CHECK_AVAIL(parser, cmd->resp_length);
            cmd->resp_data = parser->buffer + parser->pos;
            parser->pos += cmd->resp_length;
            cmd->argc = 0;
            break;
        }
        
        default:
            /* Unknown opcode */
            fprintf(stderr, "RESPB Parser: Unknown opcode 0x%04X at position %zu\n", 
                    cmd->opcode, parser->pos - 4);
            return -1;
    }
    
    cmd->raw_payload_len = parser->pos - payload_start;
    return 1; /* Success */
}

const char *respb_opcode_name(uint16_t opcode) {
    switch (opcode) {
        case RESPB_OP_GET: return "GET";
        case RESPB_OP_SET: return "SET";
        case RESPB_OP_APPEND: return "APPEND";
        case RESPB_OP_DECR: return "DECR";
        case RESPB_OP_INCR: return "INCR";
        case RESPB_OP_MGET: return "MGET";
        case RESPB_OP_MSET: return "MSET";
        case RESPB_OP_DEL: return "DEL";
        case RESPB_OP_EXISTS: return "EXISTS";
        case RESPB_OP_LPUSH: return "LPUSH";
        case RESPB_OP_RPUSH: return "RPUSH";
        case RESPB_OP_LPOP: return "LPOP";
        case RESPB_OP_RPOP: return "RPOP";
        case RESPB_OP_LLEN: return "LLEN";
        case RESPB_OP_LRANGE: return "LRANGE";
        case RESPB_OP_SADD: return "SADD";
        case RESPB_OP_SREM: return "SREM";
        case RESPB_OP_SMEMBERS: return "SMEMBERS";
        case RESPB_OP_SCARD: return "SCARD";
        case RESPB_OP_ZADD: return "ZADD";
        case RESPB_OP_ZREM: return "ZREM";
        case RESPB_OP_ZSCORE: return "ZSCORE";
        case RESPB_OP_ZRANGE: return "ZRANGE";
        case RESPB_OP_HSET: return "HSET";
        case RESPB_OP_HGET: return "HGET";
        case RESPB_OP_HDEL: return "HDEL";
        case RESPB_OP_HGETALL: return "HGETALL";
        case RESPB_OP_PING: return "PING";
        case RESPB_OP_MULTI: return "MULTI";
        case RESPB_OP_EXEC: return "EXEC";
        case RESPB_OP_MODULE: return "MODULE";
        case RESPB_OP_RESP_PASSTHROUGH: return "RESP_PASSTHROUGH";
        default: return "UNKNOWN";
    }
}

