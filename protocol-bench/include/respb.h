/*
 * RESPB Protocol Definitions and API
 * Binary protocol for Redis/Valkey commands
 */

#ifndef RESPB_H
#define RESPB_H

#include <stdint.h>
#include <stddef.h>

// RESPB Opcodes (Request commands: 0x0000-0xEFFF)
// String Operations (0x0000-0x003F)
#define RESPB_OP_GET        0x0000
#define RESPB_OP_SET        0x0001
#define RESPB_OP_APPEND     0x0002
#define RESPB_OP_DECR       0x0003
#define RESPB_OP_DECRBY     0x0004
#define RESPB_OP_GETDEL    0x0005
#define RESPB_OP_GETEX      0x0006
#define RESPB_OP_GETRANGE   0x0007
#define RESPB_OP_GETSET     0x0008
#define RESPB_OP_INCR       0x0009
#define RESPB_OP_INCRBY     0x000A
#define RESPB_OP_INCRBYFLOAT 0x000B
#define RESPB_OP_MGET       0x000C
#define RESPB_OP_MSET       0x000D
#define RESPB_OP_MSETNX     0x000E
#define RESPB_OP_PSETEX     0x000F
#define RESPB_OP_SETEX      0x0010
#define RESPB_OP_SETNX      0x0011
#define RESPB_OP_SETRANGE   0x0012
#define RESPB_OP_STRLEN     0x0013
#define RESPB_OP_SUBSTR     0x0014
#define RESPB_OP_LCS        0x0015
#define RESPB_OP_DELIFEQ    0x0016

// List Operations (0x0040-0x007F)
#define RESPB_OP_LPUSH      0x0040
#define RESPB_OP_RPUSH      0x0041
#define RESPB_OP_LPOP       0x0042
#define RESPB_OP_RPOP       0x0043
#define RESPB_OP_LLEN       0x0044
#define RESPB_OP_LRANGE     0x0045
#define RESPB_OP_LINDEX     0x0046
#define RESPB_OP_LSET       0x0047
#define RESPB_OP_LREM       0x0048
#define RESPB_OP_LTRIM      0x0049
#define RESPB_OP_LINSERT    0x004A
#define RESPB_OP_LPUSHX     0x004B
#define RESPB_OP_RPUSHX     0x004C
#define RESPB_OP_RPOPLPUSH  0x004D
#define RESPB_OP_LMOVE      0x004E
#define RESPB_OP_LMPOP      0x004F
#define RESPB_OP_LPOS       0x0050
#define RESPB_OP_BLPOP      0x0051
#define RESPB_OP_BRPOP      0x0052
#define RESPB_OP_BRPOPLPUSH 0x0053
#define RESPB_OP_BLMOVE     0x0054
#define RESPB_OP_BLMPOP     0x0055

// Set Operations (0x0080-0x00BF)
#define RESPB_OP_SADD       0x0080
#define RESPB_OP_SREM       0x0081
#define RESPB_OP_SMEMBERS   0x0082
#define RESPB_OP_SISMEMBER  0x0083
#define RESPB_OP_SCARD      0x0084
#define RESPB_OP_SPOP       0x0085
#define RESPB_OP_SRANDMEMBER 0x0086
#define RESPB_OP_SINTER     0x0087
#define RESPB_OP_SINTERSTORE 0x0088
#define RESPB_OP_SUNION     0x0089
#define RESPB_OP_SUNIONSTORE 0x008A
#define RESPB_OP_SDIFF      0x008B
#define RESPB_OP_SDIFFSTORE 0x008C
#define RESPB_OP_SMOVE      0x008D
#define RESPB_OP_SSCAN      0x008E
#define RESPB_OP_SINTERCARD 0x008F
#define RESPB_OP_SMISMEMBER 0x0090

// Sorted Set Operations (0x00C0-0x00FF)
#define RESPB_OP_ZADD       0x00C0
#define RESPB_OP_ZREM       0x00C1
#define RESPB_OP_ZCARD      0x00C2
#define RESPB_OP_ZCOUNT     0x00C3
#define RESPB_OP_ZINCRBY    0x00C4
#define RESPB_OP_ZRANGE     0x00C5
#define RESPB_OP_ZRANGEBYSCORE 0x00C6
#define RESPB_OP_ZRANGEBYLEX 0x00C7
#define RESPB_OP_ZREVRANGE  0x00C8
#define RESPB_OP_ZREVRANGEBYSCORE 0x00C9
#define RESPB_OP_ZREVRANGEBYLEX 0x00CA
#define RESPB_OP_ZRANK      0x00CB
#define RESPB_OP_ZREVRANK   0x00CC
#define RESPB_OP_ZSCORE     0x00CD
#define RESPB_OP_ZMSCORE    0x00CE
#define RESPB_OP_ZREMRANGEBYRANK 0x00CF
#define RESPB_OP_ZREMRANGEBYSCORE 0x00D0
#define RESPB_OP_ZREMRANGEBYLEX 0x00D1
#define RESPB_OP_ZLEXCOUNT  0x00D2
#define RESPB_OP_ZPOPMIN    0x00D3
#define RESPB_OP_ZPOPMAX    0x00D4
#define RESPB_OP_BZPOPMIN   0x00D5
#define RESPB_OP_BZPOPMAX   0x00D6
#define RESPB_OP_ZRANDMEMBER 0x00D7
#define RESPB_OP_ZDIFF      0x00D8
#define RESPB_OP_ZDIFFSTORE 0x00D9
#define RESPB_OP_ZINTER     0x00DA
#define RESPB_OP_ZINTERSTORE 0x00DB
#define RESPB_OP_ZINTERCARD 0x00DC
#define RESPB_OP_ZUNION     0x00DD
#define RESPB_OP_ZUNIONSTORE 0x00DE
#define RESPB_OP_ZSCAN      0x00DF
#define RESPB_OP_ZMPOP      0x00E0
#define RESPB_OP_BZMPOP     0x00E1
#define RESPB_OP_ZRANGESTORE 0x00E2

// Hash Operations (0x0100-0x013F)
#define RESPB_OP_HSET       0x0100
#define RESPB_OP_HGET       0x0101
#define RESPB_OP_HMSET      0x0102
#define RESPB_OP_HMGET      0x0103
#define RESPB_OP_HGETALL    0x0104
#define RESPB_OP_HDEL       0x0105
#define RESPB_OP_HEXISTS    0x0106
#define RESPB_OP_HINCRBY    0x0107
#define RESPB_OP_HINCRBYFLOAT 0x0108
#define RESPB_OP_HKEYS      0x0109
#define RESPB_OP_HVALS      0x010A
#define RESPB_OP_HLEN       0x010B
#define RESPB_OP_HSETNX     0x010C
#define RESPB_OP_HSTRLEN    0x010D
#define RESPB_OP_HSCAN      0x010E
#define RESPB_OP_HRANDFIELD 0x010F
#define RESPB_OP_HEXPIRE    0x0110
#define RESPB_OP_HEXPIREAT  0x0111
#define RESPB_OP_HEXPIRETIME 0x0112
#define RESPB_OP_HPEXPIRE   0x0113
#define RESPB_OP_HPEXPIREAT 0x0114
#define RESPB_OP_HPEXPIRETIME 0x0115
#define RESPB_OP_HPTTL      0x0116
#define RESPB_OP_HTTL       0x0117
#define RESPB_OP_HPERSIST   0x0118
#define RESPB_OP_HGETEX     0x0119
#define RESPB_OP_HSETEX     0x011A

// Bitmap Operations (0x0140-0x015F)
#define RESPB_OP_SETBIT     0x0140
#define RESPB_OP_GETBIT     0x0141
#define RESPB_OP_BITCOUNT   0x0142
#define RESPB_OP_BITPOS     0x0143
#define RESPB_OP_BITOP      0x0144
#define RESPB_OP_BITFIELD   0x0145
#define RESPB_OP_BITFIELD_RO 0x0146

// HyperLogLog Operations (0x0160-0x017F)
#define RESPB_OP_PFADD      0x0160
#define RESPB_OP_PFCOUNT    0x0161
#define RESPB_OP_PFMERGE    0x0162
#define RESPB_OP_PFDEBUG    0x0163
#define RESPB_OP_PFSELFTEST 0x0164

// Geospatial Operations (0x0180-0x01BF)
#define RESPB_OP_GEOADD     0x0180
#define RESPB_OP_GEODIST    0x0181
#define RESPB_OP_GEOHASH    0x0182
#define RESPB_OP_GEOPOS     0x0183
#define RESPB_OP_GEORADIUS  0x0184
#define RESPB_OP_GEORADIUSBYMEMBER 0x0185
#define RESPB_OP_GEORADIUS_RO 0x0186
#define RESPB_OP_GEORADIUSBYMEMBER_RO 0x0187
#define RESPB_OP_GEOSEARCH  0x0188
#define RESPB_OP_GEOSEARCHSTORE 0x0189

// Stream Operations (0x01C0-0x01FF)
#define RESPB_OP_XADD       0x01C0
#define RESPB_OP_XLEN       0x01C1
#define RESPB_OP_XRANGE     0x01C2
#define RESPB_OP_XREVRANGE  0x01C3
#define RESPB_OP_XREAD      0x01C4
#define RESPB_OP_XREADGROUP 0x01C5
#define RESPB_OP_XDEL       0x01C6
#define RESPB_OP_XTRIM      0x01C7
#define RESPB_OP_XACK       0x01C8
#define RESPB_OP_XPENDING   0x01C9
#define RESPB_OP_XCLAIM     0x01CA
#define RESPB_OP_XAUTOCLAIM 0x01CB
#define RESPB_OP_XINFO      0x01CC
#define RESPB_OP_XGROUP     0x01CD
#define RESPB_OP_XSETID     0x01CE

// Generic Key Operations (0x02C0-0x02FF)
#define RESPB_OP_DEL        0x02C0
#define RESPB_OP_UNLINK     0x02C1
#define RESPB_OP_EXISTS     0x02C2
#define RESPB_OP_EXPIRE     0x02C3
#define RESPB_OP_EXPIREAT   0x02C4
#define RESPB_OP_EXPIRETIME 0x02C5
#define RESPB_OP_PEXPIRE    0x02C6
#define RESPB_OP_PEXPIREAT 0x02C7
#define RESPB_OP_PEXPIRETIME 0x02C8
#define RESPB_OP_TTL        0x02C9
#define RESPB_OP_PTTL       0x02CA
#define RESPB_OP_PERSIST    0x02CB
#define RESPB_OP_KEYS       0x02CC
#define RESPB_OP_SCAN       0x02CD
#define RESPB_OP_RANDOMKEY  0x02CE
#define RESPB_OP_RENAME     0x02CF
#define RESPB_OP_RENAMENX   0x02D0
#define RESPB_OP_TYPE       0x02D1
#define RESPB_OP_DUMP       0x02D2
#define RESPB_OP_RESTORE    0x02D3
#define RESPB_OP_MIGRATE    0x02D4
#define RESPB_OP_MOVE       0x02D5
#define RESPB_OP_COPY       0x02D6
#define RESPB_OP_SORT       0x02D7
#define RESPB_OP_SORT_RO    0x02D8
#define RESPB_OP_TOUCH      0x02D9
#define RESPB_OP_OBJECT     0x02DA
#define RESPB_OP_WAIT       0x02DB
#define RESPB_OP_WAITAOF    0x02DC

// Connection Management (0x0300-0x033F)
#define RESPB_OP_PING       0x0300
#define RESPB_OP_ECHO       0x0301
#define RESPB_OP_AUTH       0x0302
#define RESPB_OP_SELECT     0x0303
#define RESPB_OP_QUIT       0x0304
#define RESPB_OP_HELLO      0x0305
#define RESPB_OP_RESET      0x0306
#define RESPB_OP_CLIENT     0x0307

// Transaction Operations (0x0240-0x025F)
#define RESPB_OP_MULTI      0x0240
#define RESPB_OP_EXEC       0x0241
#define RESPB_OP_DISCARD    0x0242
#define RESPB_OP_WATCH      0x0243
#define RESPB_OP_UNWATCH    0x0244

// Scripting and Functions (0x0260-0x02BF)
#define RESPB_OP_EVAL       0x0260
#define RESPB_OP_EVALSHA    0x0261
#define RESPB_OP_EVAL_RO    0x0262
#define RESPB_OP_EVALSHA_RO 0x0263
#define RESPB_OP_SCRIPT     0x0264
#define RESPB_OP_FCALL      0x0265
#define RESPB_OP_FCALL_RO   0x0266
#define RESPB_OP_FUNCTION   0x0267

// Cluster Management (0x0340-0x03BF)
#define RESPB_OP_CLUSTER    0x0340
#define RESPB_OP_READONLY   0x0341
#define RESPB_OP_READWRITE  0x0342
#define RESPB_OP_ASKING     0x0343

// Server Management (0x03C0-0x04FF)
#define RESPB_OP_DBSIZE     0x03C0
#define RESPB_OP_FLUSHDB    0x03C1
#define RESPB_OP_FLUSHALL   0x03C2
#define RESPB_OP_SAVE       0x03C3
#define RESPB_OP_BGSAVE     0x03C4
#define RESPB_OP_BGREWRITEAOF 0x03C5
#define RESPB_OP_LASTSAVE   0x03C6
#define RESPB_OP_SHUTDOWN   0x03C7
#define RESPB_OP_INFO       0x03C8
#define RESPB_OP_CONFIG     0x03C9
#define RESPB_OP_COMMAND    0x03CA
#define RESPB_OP_TIME       0x03CB
#define RESPB_OP_ROLE       0x03CC
#define RESPB_OP_REPLICAOF  0x03CD
#define RESPB_OP_SLAVEOF    0x03CE
#define RESPB_OP_MONITOR    0x03CF
#define RESPB_OP_DEBUG      0x03D0
#define RESPB_OP_SYNC       0x03D1
#define RESPB_OP_PSYNC      0x03D2
#define RESPB_OP_REPLCONF   0x03D3
#define RESPB_OP_SLOWLOG    0x03D4
#define RESPB_OP_LATENCY    0x03D5
#define RESPB_OP_MEMORY     0x03D6
#define RESPB_OP_MODULE_CMD 0x03D7
#define RESPB_OP_ACL        0x03D8
#define RESPB_OP_FAILOVER   0x03D9
#define RESPB_OP_SWAPDB     0x03DA
#define RESPB_OP_LOLWUT     0x03DB
#define RESPB_OP_RESTORE_ASKING 0x03DC
#define RESPB_OP_COMMANDLOG 0x03DD

// Pub/Sub Operations (0x0200-0x023F)
#define RESPB_OP_PUBLISH    0x0200
#define RESPB_OP_SUBSCRIBE  0x0201
#define RESPB_OP_UNSUBSCRIBE 0x0202
#define RESPB_OP_PSUBSCRIBE 0x0203
#define RESPB_OP_PUNSUBSCRIBE 0x0204
#define RESPB_OP_PUBSUB     0x0205
#define RESPB_OP_SPUBLISH   0x0206
#define RESPB_OP_SSUBSCRIBE 0x0207
#define RESPB_OP_SUNSUBSCRIBE 0x0208

// Module and special opcodes
#define RESPB_OP_MODULE     0xF000
#define RESPB_OP_RESP_PASSTHROUGH 0xFFFF

// Module IDs (high 16 bits of 4-byte subcommand)
#define RESPB_MODULE_JSON   0x0000
#define RESPB_MODULE_BF     0x0001
#define RESPB_MODULE_FT     0x0002

// Response opcodes (0x8000-0xFFFE)
#define RESPB_RESP_OK       0x8000
#define RESPB_RESP_ERROR    0x8001
#define RESPB_RESP_NULL     0x8002
#define RESPB_RESP_INT      0x8003
#define RESPB_RESP_BULK     0x8004
#define RESPB_RESP_ARRAY    0x8005

// Maximum arguments per command
#define RESPB_MAX_ARGS      64

// Command argument
typedef struct {
    const uint8_t *data;
    size_t len;
} respb_arg_t;

// Module command frame (8-byte header)
typedef struct {
    uint16_t opcode;        // Always 0xF000
    uint16_t mux_id;
    uint32_t subcommand;    // Module ID (high 16 bits) | Command ID (low 16 bits)
} respb_module_frame_t;

// RESP passthrough frame (8-byte header)
typedef struct {
    uint16_t opcode;        // Always 0xFFFF
    uint16_t mux_id;
    uint32_t resp_length;   // Length of following RESP text data
} respb_resp_passthrough_t;

// Parsed command
typedef struct {
    uint16_t opcode;
    uint16_t mux_id;
    size_t argc;
    respb_arg_t args[RESPB_MAX_ARGS];
    const uint8_t *raw_payload;
    size_t raw_payload_len;
    // Module command fields (when opcode == RESPB_OP_MODULE)
    uint32_t module_subcommand;
    uint16_t module_id;     // Extracted from subcommand high 16 bits
    uint16_t command_id;    // Extracted from subcommand low 16 bits
    // RESP passthrough fields (when opcode == RESPB_OP_RESP_PASSTHROUGH)
    uint32_t resp_length;
    const uint8_t *resp_data;
} respb_command_t;

// Parser state
typedef struct {
    const uint8_t *buffer;
    size_t buffer_len;
    size_t pos;
} respb_parser_t;

// Parser functions
void respb_parser_init(respb_parser_t *parser, const uint8_t *buf, size_t len);
int respb_parse_header(respb_parser_t *parser, uint16_t *opcode, uint16_t *mux_id);
int respb_parse_command(respb_parser_t *parser, respb_command_t *cmd);
const char *respb_opcode_name(uint16_t opcode);

// Serializer functions
void respb_write_u16(uint8_t *buf, uint16_t val);
void respb_write_u32(uint8_t *buf, uint32_t val);
void respb_write_u64(uint8_t *buf, uint64_t val);
size_t respb_serialize_header(uint8_t *buf, uint16_t opcode, uint16_t mux_id);
size_t respb_serialize_command(uint8_t *buf, size_t buf_len, const respb_command_t *cmd);

// Helper functions for reading
static inline uint16_t respb_read_u16(const uint8_t *buf) {
    return ((uint16_t)buf[0] << 8) | buf[1];
}

static inline uint32_t respb_read_u32(const uint8_t *buf) {
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) | buf[3];
}

static inline uint64_t respb_read_u64(const uint8_t *buf) {
    return ((uint64_t)buf[0] << 56) | ((uint64_t)buf[1] << 48) |
           ((uint64_t)buf[2] << 40) | ((uint64_t)buf[3] << 32) |
           ((uint64_t)buf[4] << 24) | ((uint64_t)buf[5] << 16) |
           ((uint64_t)buf[6] << 8) | buf[7];
}

#endif // RESPB_H
