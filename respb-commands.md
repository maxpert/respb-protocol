# RESPB Command Opcode Mapping for Valkey

This document maps all Valkey commands to RESPB binary protocol opcodes, following the [RESPB specification](respb-specs.md).

## Opcode Allocation Strategy

The RESPB opcode space is organized into distinct ranges for core commands, module commands, and special functions.

Core Commands: 0x0000 to 0xEFFF

This range contains all core Valkey commands. These commands use a 4-byte header consisting of opcode and mux ID. Commands are grouped by category with power-of-2 boundaries for future expansion.

Module Commands: 0xF000

The opcode 0xF000 indicates a module command. Module commands use an 8-byte header: 2 bytes for the 0xF000 opcode, 2 bytes for mux ID, and 4 bytes for the module subcommand. The 4-byte subcommand encodes module ID in the high 16 bits and command ID in the low 16 bits. This design supports up to 65,536 modules, each with up to 65,536 commands.

RESP Passthrough: 0xFFFF

The opcode 0xFFFF enables sending plain text RESP commands over a binary connection. The frame format is 8 bytes: 2 bytes for 0xFFFF opcode, 2 bytes for mux ID, 4 bytes for RESP data length, followed by the raw RESP text command.

Reserved Range: 0xF001 to 0xFFFE

This range is reserved for future protocol extensions.

Response Opcodes: 0x8000 to 0xFFFE

Response opcodes use distinct ranges from request opcodes to enable clear differentiation.

## Encoding Conventions

- **2-byte length prefix:** Used for keys, field names, member names, small strings (max 65,535 bytes)
- **4-byte length prefix:** Used for values, large payloads (max 4,294,967,295 bytes)
- **8-byte integers:** Used for counts, scores, numeric values (signed 64-bit)
- **8-byte floats:** Used for floating-point scores (IEEE 754 binary64)
- **1-byte flags:** Used for option flags (NX, XX, GT, LT, etc.)

---

## String Operations (0x0000 to 0x003F, 64 opcodes)

```
Opcode         Command        Payload Format
=============  =============  ============================================================
0x0000         GET            [2B keylen][key]
0x0001         SET            [2B keylen][key][4B vallen][value][1B flags][8B expiry?]
0x0002         APPEND         [2B keylen][key][4B datalen][data]
0x0003         DECR           [2B keylen][key]
0x0004         DECRBY         [2B keylen][key][8B decrement]
0x0005         GETDEL         [2B keylen][key]
0x0006         GETEX          [2B keylen][key][1B flags][8B expiry?]
0x0007         GETRANGE       [2B keylen][key][8B start][8B end]
0x0008         GETSET         [2B keylen][key][4B vallen][value]
0x0009         INCR           [2B keylen][key]
0x000A         INCRBY         [2B keylen][key][8B increment]
0x000B         INCRBYFLOAT    [2B keylen][key][8B float]
0x000C         MGET           [2B count][2B keylen][key]...
0x000D         MSET           [2B count]([2B keylen][key][4B vallen][value])...
0x000E         MSETNX         [2B count]([2B keylen][key][4B vallen][value])...
0x000F         PSETEX         [2B keylen][key][8B millis][4B vallen][value]
0x0010         SETEX          [2B keylen][key][8B seconds][4B vallen][value]
0x0011         SETNX          [2B keylen][key][4B vallen][value]
0x0012         SETRANGE       [2B keylen][key][8B offset][4B vallen][value]
0x0013         STRLEN         [2B keylen][key]
0x0014         SUBSTR         [2B keylen][key][8B start][8B end]
0x0015         LCS            [2B key1len][key1][2B key2len][key2][1B flags]
0x0016         DELIFEQ        [2B keylen][key][4B vallen][value]
0x0017-0x003F                 Reserved for future string commands
```

---

## List Operations (0x0040 to 0x007F, 64 opcodes)

```
Opcode         Command        Payload Format
=============  =============  =============================================================================
0x0040         LPUSH          [2B keylen][key][2B count]([2B elemlen][elem])...
0x0041         RPUSH          [2B keylen][key][2B count]([2B elemlen][elem])...
0x0042         LPOP           [2B keylen][key][2B count?]
0x0043         RPOP           [2B keylen][key][2B count?]
0x0044         LLEN           [2B keylen][key]
0x0045         LRANGE         [2B keylen][key][8B start][8B stop]
0x0046         LINDEX         [2B keylen][key][8B index]
0x0047         LSET           [2B keylen][key][8B index][2B elemlen][elem]
0x0048         LREM           [2B keylen][key][8B count][2B elemlen][elem]
0x0049         LTRIM          [2B keylen][key][8B start][8B stop]
0x004A         LINSERT        [2B keylen][key][1B before_after][2B pivotlen][pivot][2B elemlen][elem]
0x004B         LPUSHX         [2B keylen][key][2B count]([2B elemlen][elem])...
0x004C         RPUSHX         [2B keylen][key][2B count]([2B elemlen][elem])...
0x004D         RPOPLPUSH      [2B srclen][src][2B dstlen][dst]
0x004E         LMOVE          [2B srclen][src][2B dstlen][dst][1B wherefrom][1B whereto]
0x004F         LMPOP          [2B numkeys]([2B keylen][key])...[1B left_right][2B count?]
0x0050         LPOS           [2B keylen][key][2B elemlen][elem][8B rank?][2B count?][8B maxlen?]
0x0051         BLPOP          [2B numkeys]([2B keylen][key])...[8B timeout]
0x0052         BRPOP          [2B numkeys]([2B keylen][key])...[8B timeout]
0x0053         BRPOPLPUSH     [2B srclen][src][2B dstlen][dst][8B timeout]
0x0054         BLMOVE         [2B srclen][src][2B dstlen][dst][1B wherefrom][1B whereto][8B timeout]
0x0055         BLMPOP         [8B timeout][2B numkeys]([2B keylen][key])...[1B left_right][2B count?]
0x0056-0x007F                 Reserved for future list commands
```

---

## Set Operations (0x0080 - 0x00BF, 64 opcodes)

```
Opcode         Command        Payload Format
=============  =============  =============================================================================
0x0080         SADD           [2B keylen][key][2B count]([2B memberlen][member])...
0x0081         SREM           [2B keylen][key][2B count]([2B memberlen][member])...
0x0082         SMEMBERS       [2B keylen][key]
0x0083         SISMEMBER      [2B keylen][key][2B memberlen][member]
0x0084         SCARD          [2B keylen][key]
0x0085         SPOP           [2B keylen][key][2B count?]
0x0086         SRANDMEMBER    [2B keylen][key][8B count?]
0x0087         SINTER         [2B numkeys]([2B keylen][key])...
0x0088         SINTERSTORE    [2B dstlen][dst][2B numkeys]([2B keylen][key])...
0x0089         SUNION         [2B numkeys]([2B keylen][key])...
0x008A         SUNIONSTORE    [2B dstlen][dst][2B numkeys]([2B keylen][key])...
0x008B         SDIFF          [2B numkeys]([2B keylen][key])...
0x008C         SDIFFSTORE     [2B dstlen][dst][2B numkeys]([2B keylen][key])...
0x008D         SMOVE          [2B srclen][src][2B dstlen][dst][2B memberlen][member]
0x008E         SSCAN          [2B keylen][key][8B cursor][2B patternlen?][pattern?][8B count?]
0x008F         SINTERCARD     [2B numkeys]([2B keylen][key])...[8B limit?]
0x0090         SMISMEMBER     [2B keylen][key][2B count]([2B memberlen][member])...
0x0091-0x00BF                 Reserved for future set commands
```

---

## Sorted Set Operations (0x00C0 - 0x00FF, 64 opcodes)

```
Opcode         Command            Payload Format
=============  =================  =============================================================================
0x00C0         ZADD               [2B keylen][key][1B flags][2B count]([8B score][2B memberlen][member])...
0x00C1         ZREM               [2B keylen][key][2B count]([2B memberlen][member])...
0x00C2         ZCARD              [2B keylen][key]
0x00C3         ZCOUNT             [2B keylen][key][8B min][8B max]
0x00C4         ZINCRBY            [2B keylen][key][8B increment][2B memberlen][member]
0x00C5         ZRANGE             [2B keylen][key][8B start][8B stop][1B flags]
0x00C6         ZRANGEBYSCORE      [2B keylen][key][8B min][8B max][1B flags]
0x00C7         ZRANGEBYLEX        [2B keylen][key][2B minlen][min][2B maxlen][max][8B offset?][8B count?]
0x00C8         ZREVRANGE          [2B keylen][key][8B start][8B stop][1B withscores]
0x00C9         ZREVRANGEBYSCORE   [2B keylen][key][8B max][8B min][1B flags][8B offset?][8B count?]
0x00CA         ZREVRANGEBYLEX     [2B keylen][key][2B maxlen][max][2B minlen][min][8B offset?][8B count?]
0x00CB         ZRANK              [2B keylen][key][2B memberlen][member][1B withscore]
0x00CC         ZREVRANK           [2B keylen][key][2B memberlen][member][1B withscore]
0x00CD         ZSCORE             [2B keylen][key][2B memberlen][member]
0x00CE         ZMSCORE            [2B keylen][key][2B count]([2B memberlen][member])...
0x00CF         ZREMRANGEBYRANK    [2B keylen][key][8B start][8B stop]
0x00D0         ZREMRANGEBYSCORE   [2B keylen][key][8B min][8B max]
0x00D1         ZREMRANGEBYLEX     [2B keylen][key][2B minlen][min][2B maxlen][max]
0x00D2         ZLEXCOUNT          [2B keylen][key][2B minlen][min][2B maxlen][max]
0x00D3         ZPOPMIN            [2B keylen][key][2B count?]
0x00D4         ZPOPMAX            [2B keylen][key][2B count?]
0x00D5         BZPOPMIN           [2B numkeys]([2B keylen][key])...[8B timeout]
0x00D6         BZPOPMAX           [2B numkeys]([2B keylen][key])...[8B timeout]
0x00D7         ZRANDMEMBER        [2B keylen][key][2B count?][1B withscores]
0x00D8         ZDIFF              [2B numkeys]([2B keylen][key])...[1B withscores]
0x00D9         ZDIFFSTORE         [2B dstlen][dst][2B numkeys]([2B keylen][key])...
0x00DA         ZINTER             [2B numkeys]([2B keylen][key])...[1B flags]
0x00DB         ZINTERSTORE        [2B dstlen][dst][2B numkeys]([2B keylen][key])...[1B flags]
0x00DC         ZINTERCARD         [2B numkeys]([2B keylen][key])...[8B limit?]
0x00DD         ZUNION             [2B numkeys]([2B keylen][key])...[1B flags]
0x00DE         ZUNIONSTORE        [2B dstlen][dst][2B numkeys]([2B keylen][key])...[1B flags]
0x00DF         ZSCAN              [2B keylen][key][8B cursor][2B patternlen?][pattern?][8B count?][1B noscores]
0x00E0         ZMPOP              [2B numkeys]([2B keylen][key])...[1B min_max][2B count?]
0x00E1         BZMPOP             [8B timeout][2B numkeys]([2B keylen][key])...[1B min_max][2B count?]
0x00E2         ZRANGESTORE        [2B dstlen][dst][2B srclen][src][8B min][8B max][1B flags]
0x00E3-0x00FF                     Reserved for future sorted set commands
```

---

## Hash Operations (0x0100 - 0x013F, 64 opcodes)

```
Opcode         Command        Payload Format
=============  =============  =============================================================================
0x0100         HSET           [2B keylen][key][2B count]([2B fieldlen][field][4B vallen][value])...
0x0101         HGET           [2B keylen][key][2B fieldlen][field]
0x0102         HMSET          [2B keylen][key][2B count]([2B fieldlen][field][4B vallen][value])...
0x0103         HMGET          [2B keylen][key][2B count]([2B fieldlen][field])...
0x0104         HGETALL        [2B keylen][key]
0x0105         HDEL            [2B keylen][key][2B count]([2B fieldlen][field])...
0x0106         HEXISTS        [2B keylen][key][2B fieldlen][field]
0x0107         HINCRBY         [2B keylen][key][2B fieldlen][field][8B increment]
0x0108         HINCRBYFLOAT    [2B keylen][key][2B fieldlen][field][8B float]
0x0109         HKEYS           [2B keylen][key]
0x010A         HVALS           [2B keylen][key]
0x010B         HLEN            [2B keylen][key]
0x010C         HSETNX          [2B keylen][key][2B fieldlen][field][4B vallen][value]
0x010D         HSTRLEN         [2B keylen][key][2B fieldlen][field]
0x010E         HSCAN           [2B keylen][key][8B cursor][2B patternlen?][pattern?][8B count?][1B novalues]
0x010F         HRANDFIELD      [2B keylen][key][2B count?][1B withvalues]
0x0110         HEXPIRE         [2B keylen][key][8B seconds][1B flags][2B numfields]([2B fieldlen][field])...
0x0111         HEXPIREAT        [2B keylen][key][8B timestamp][1B flags][2B numfields]([2B fieldlen][field])...
0x0112         HEXPIRETIME     [2B keylen][key][2B numfields]([2B fieldlen][field])...
0x0113         HPEXPIRE         [2B keylen][key][8B millis][1B flags][2B numfields]([2B fieldlen][field])...
0x0114         HPEXPIREAT       [2B keylen][key][8B timestamp][1B flags][2B numfields]([2B fieldlen][field])...
0x0115         HPEXPIRETIME     [2B keylen][key][2B numfields]([2B fieldlen][field])...
0x0116         HPTTL            [2B keylen][key][2B numfields]([2B fieldlen][field])...
0x0117         HTTL             [2B keylen][key][2B numfields]([2B fieldlen][field])...
0x0118         HPERSIST         [2B keylen][key][2B numfields]([2B fieldlen][field])...
0x0119         HGETEX           [2B keylen][key][1B flags][8B expiry?][2B numfields]([2B fieldlen][field])...
0x011A         HSETEX           [2B keylen][key][1B flags][8B expiry?][2B numfields]([2B fieldlen][field][4B vallen][value])...
0x011B-0x013F                  Reserved for future hash commands
```

---

## Bitmap Operations (0x0140 - 0x015F, 32 opcodes)

```
Opcode         Command        Payload Format
=============  =============  =============================================================================
0x0140         SETBIT          [2B keylen][key][8B offset][1B value]
0x0141         GETBIT          [2B keylen][key][8B offset]
0x0142         BITCOUNT        [2B keylen][key][8B start?][8B end?][1B unit]
0x0143         BITPOS          [2B keylen][key][1B bit][8B start?][8B end?][1B unit]
0x0144         BITOP           [1B operation][2B dstlen][dst][2B numkeys]([2B keylen][key])...
0x0145         BITFIELD        [2B keylen][key][2B count]([1B op][2B args]...)...
0x0146         BITFIELD_RO     [2B keylen][key][2B count]([1B op][2B args]...)...
0x0147-0x015F                  Reserved for future bitmap commands
```

---

## HyperLogLog Operations (0x0160 - 0x017F, 32 opcodes)

```
Opcode         Command        Payload Format
=============  =============  =============================================================================
0x0160         PFADD           [2B keylen][key][2B count]([2B elemlen][elem])...
0x0161         PFCOUNT         [2B numkeys]([2B keylen][key])...
0x0162         PFMERGE         [2B dstlen][dst][2B numkeys]([2B keylen][key])...
0x0163         PFDEBUG         [2B subcmdlen][subcmd][2B keylen][key]
0x0164         PFSELFTEST      
0x0165-0x017F                  Reserved for future HyperLogLog commands
```

---

## Geospatial Operations (0x0180 - 0x01BF, 64 opcodes)

```
Opcode         Command            Payload Format
=============  =================  ================================================================================
0x0180         GEOADD               [2B keylen][key][1B flags][2B count]([8B longitude][8B latitude][2B memberlen][member])...
0x0181         GEODIST              [2B keylen][key][2B mem1len][mem1][2B mem2len][mem2][1B unit]
0x0182         GEOHASH              [2B keylen][key][2B count]([2B memberlen][member])...
0x0183         GEOPOS               [2B keylen][key][2B count]([2B memberlen][member])...
0x0184         GEORADIUS            [2B keylen][key][8B longitude][8B latitude][8B radius][1B unit][1B flags]
0x0185         GEORADIUSBYMEMBER    [2B keylen][key][2B memberlen][member][8B radius][1B unit][1B flags]
0x0186         GEORADIUS_RO         [2B keylen][key][8B longitude][8B latitude][8B radius][1B unit][1B flags]
0x0187         GEORADIUSBYMEMBER_RO [2B keylen][key][2B memberlen][member][8B radius][1B unit][1B flags]
0x0188         GEOSEARCH            [2B keylen][key][...complex payload with flags]
0x0189         GEOSEARCHSTORE       [2B dstlen][dst][2B srclen][src][...complex payload with flags]
0x018A-0x01BF                       Reserved for future geospatial commands
```

---

## Stream Operations (0x01C0 - 0x01FF, 64 opcodes)

```
Opcode         Command        Payload Format
=============  =============  ==========================================================================================
0x01C0         XADD            [2B keylen][key][2B idlen][id][2B count]([2B fieldlen][field][4B vallen][value])...
0x01C1         XLEN            [2B keylen][key]
0x01C2         XRANGE          [2B keylen][key][2B startlen][start][2B endlen][end][8B count?]
0x01C3         XREVRANGE       [2B keylen][key][2B endlen][end][2B startlen][start][8B count?]
0x01C4         XREAD           [8B count?][8B block?][2B numkeys]([2B keylen][key][2B idlen][id])...
0x01C5         XREADGROUP      [2B grouplen][group][2B consumerlen][consumer][8B count?][8B block?][1B noack][2B numkeys]([2B keylen][key][2B idlen][id])...
0x01C6         XDEL            [2B keylen][key][2B count]([2B idlen][id])...
0x01C7         XTRIM           [2B keylen][key][1B strategy][8B threshold][1B flags]
0x01C8         XACK            [2B keylen][key][2B grouplen][group][2B count]([2B idlen][id])...
0x01C9         XPENDING        [2B keylen][key][2B grouplen][group][8B idle?][2B startlen?][start?][2B endlen?][end?][8B count?][2B consumerlen?][consumer?]
0x01CA         XCLAIM          [2B keylen][key][2B grouplen][group][2B consumerlen][consumer][8B min_idle][2B count]([2B idlen][id])...[1B flags]
0x01CB         XAUTOCLAIM      [2B keylen][key][2B grouplen][group][2B consumerlen][consumer][8B min_idle][2B startlen][start][8B count?][1B justid]
0x01CC         XINFO           [1B subcommand][2B keylen][key][additional args...]
0x01CD         XGROUP          [1B subcommand][2B keylen][key][additional args...]
0x01CE         XSETID          [2B keylen][key][2B idlen][id][8B entries_added?][2B maxdeletlen?][maxdeleteid?]
0x01CF-0x01FF                  Reserved for future stream commands
```

---

## Pub/Sub Operations (0x0200 - 0x023F, 64 opcodes)

```
Opcode         Command        Payload Format
=============  =============  =============================================================================
0x0200         PUBLISH         [2B channellen][channel][4B msglen][message]
0x0201         SUBSCRIBE       [2B count]([2B channellen][channel])...
0x0202         UNSUBSCRIBE     [2B count]([2B channellen][channel])...
0x0203         PSUBSCRIBE      [2B count]([2B patternlen][pattern])...
0x0204         PUNSUBSCRIBE    [2B count]([2B patternlen][pattern])...
0x0205         PUBSUB          [1B subcommand][additional args...]
0x0206         SPUBLISH         [2B channellen][channel][4B msglen][message]
0x0207         SSUBSCRIBE      [2B count]([2B channellen][channel])...
0x0208         SUNSUBSCRIBE    [2B count]([2B channellen][channel])...
0x0209-0x023F                  Reserved for future pub/sub commands
```

---

## Transaction Operations (0x0240 - 0x025F, 32 opcodes)

```
Opcode         Command        Payload Format
=============  =============  =============================================================================
0x0240         MULTI          
0x0241         EXEC            
0x0242         DISCARD        
0x0243         WATCH           [2B numkeys]([2B keylen][key])...
0x0244         UNWATCH         
0x0245-0x025F                 Reserved for future transaction commands
```

---

## Scripting and Functions (0x0260 - 0x02BF, 96 opcodes)

```
Opcode         Command        Payload Format
=============  =============  =============================================================================
0x0260         EVAL           [4B scriptlen][script][2B numkeys]([2B keylen][key])...[2B numargs]([2B arglen][arg])...
0x0261         EVALSHA         [2B sha1len][sha1][2B numkeys]([2B keylen][key])...[2B numargs]([2B arglen][arg])...
0x0262         EVAL_RO         [4B scriptlen][script][2B numkeys]([2B keylen][key])...[2B numargs]([2B arglen][arg])...
0x0263         EVALSHA_RO      [2B sha1len][sha1][2B numkeys]([2B keylen][key])...[2B numargs]([2B arglen][arg])...
0x0264         SCRIPT          [1B subcommand][additional args...]
0x0265         FCALL           [2B funclen][function][2B numkeys]([2B keylen][key])...[2B numargs]([2B arglen][arg])...
0x0266         FCALL_RO        [2B funclen][function][2B numkeys]([2B keylen][key])...[2B numargs]([2B arglen][arg])...
0x0267         FUNCTION        [1B subcommand][additional args...]
0x0268-0x02BF                  Reserved for future scripting commands
```

---

## Generic Key Operations (0x02C0 - 0x02FF, 64 opcodes)

```
Opcode         Command        Payload Format
=============  =============  =============================================================================
0x02C0         DEL            [2B numkeys]([2B keylen][key])...
0x02C1         UNLINK         [2B numkeys]([2B keylen][key])...
0x02C2         EXISTS         [2B numkeys]([2B keylen][key])...
0x02C3         EXPIRE         [2B keylen][key][8B seconds][1B flags]
0x02C4         EXPIREAT       [2B keylen][key][8B timestamp][1B flags]
0x02C5         EXPIRETIME     [2B keylen][key]
0x02C6         PEXPIRE        [2B keylen][key][8B millis][1B flags]
0x02C7         PEXPIREAT      [2B keylen][key][8B timestamp][1B flags]
0x02C8         PEXPIRETIME    [2B keylen][key]
0x02C9         TTL            [2B keylen][key]
0x02CA         PTTL           [2B keylen][key]
0x02CB         PERSIST        [2B keylen][key]
0x02CC         KEYS           [2B patternlen][pattern]
0x02CD         SCAN           [8B cursor][2B patternlen?][pattern?][8B count?][2B typelen?][type?]
0x02CE         RANDOMKEY      
0x02CF         RENAME         [2B keylen][key][2B newkeylen][newkey]
0x02D0         RENAMENX       [2B keylen][key][2B newkeylen][newkey]
0x02D1         TYPE           [2B keylen][key]
0x02D2         DUMP           [2B keylen][key]
0x02D3         RESTORE        [2B keylen][key][8B ttl][4B datalen][data][1B flags]
0x02D4         MIGRATE        [2B hostlen][host][2B port][2B keylen][key][2B db][8B timeout][1B flags]
0x02D5         MOVE           [2B keylen][key][2B db]
0x02D6         COPY           [2B srclen][src][2B dstlen][dst][2B db?][1B replace]
0x02D7         SORT           [2B keylen][key][...complex sorting options]
0x02D8         SORT_RO        [2B keylen][key][...complex sorting options]
0x02D9         TOUCH          [2B numkeys]([2B keylen][key])...
0x02DA         OBJECT         [1B subcommand][2B keylen][key]
0x02DB         WAIT           [8B numreplicas][8B timeout]
0x02DC         WAITAOF        [8B numlocal][8B numreplicas][8B timeout]
0x02DD-0x02FF                 Reserved for future generic commands
```

---

## Connection Management (0x0300 - 0x033F, 64 opcodes)

```
Opcode         Command        Payload Format
=============  =============  =============================================================================
0x0300         PING            [2B msglen?][message?]
0x0301         ECHO            [2B msglen][message]
0x0302         AUTH            [2B userlen?][username?][2B passlen][password]
0x0303         SELECT          [2B dbindex]
0x0304         QUIT            
0x0305         HELLO           [1B protover][2B userlen?][username?][2B passlen?][password?][2B clientnamelen?][clientname?]
0x0306         RESET           
0x0307         CLIENT          [1B subcommand][additional args...]
0x0308-0x033F                 Reserved for future connection commands
```

---

## Cluster Management (0x0340 - 0x03BF, 128 opcodes)

```
Opcode         Command        Payload Format
=============  =============  =============================================================================
0x0340         CLUSTER        [1B subcommand][additional args...]
0x0341         READONLY       
0x0342         READWRITE      
0x0343         ASKING         
0x0344-0x03BF                 Reserved for future cluster commands
```

---

## Server Management (0x03C0 - 0x04FF, 320 opcodes)

```
Opcode         Command        Payload Format
=============  =============  =============================================================================
0x03C0         DBSIZE         
0x03C1         FLUSHDB         [1B async_sync]
0x03C2         FLUSHALL        [1B async_sync]
0x03C3         SAVE            
0x03C4         BGSAVE          [1B flags]
0x03C5         BGREWRITEAOF    
0x03C6         LASTSAVE        
0x03C7         SHUTDOWN        [1B flags]
0x03C8         INFO            [2B count]([2B sectionlen][section])...
0x03C9         CONFIG          [1B subcommand][additional args...]
0x03CA         COMMAND         [1B subcommand][additional args...]
0x03CB         TIME            
0x03CC         ROLE            
0x03CD         REPLICAOF       [2B hostlen][host][2B port]
0x03CE         SLAVEOF         [2B hostlen][host][2B port]
0x03CF         MONITOR         
0x03D0         DEBUG           [1B subcommand][additional args...]
0x03D1         SYNC            
0x03D2         PSYNC           [2B replidlen][replicationid][8B offset]
0x03D3         REPLCONF        [2B count]([2B arglen][arg])...
0x03D4         SLOWLOG         [1B subcommand][8B count?]
0x03D5         LATENCY         [1B subcommand][additional args...]
0x03D6         MEMORY          [1B subcommand][additional args...]
0x03D7         MODULE          [1B subcommand][additional args...]
0x03D8         ACL             [1B subcommand][additional args...]
0x03D9         FAILOVER        [1B flags][2B hostlen?][host?][2B port?][8B timeout?]
0x03DA         SWAPDB          [2B db1][2B db2]
0x03DB         LOLWUT          [2B count]([2B arglen][arg])...
0x03DC         RESTORE-ASKING  [2B keylen][key][8B ttl][4B datalen][data][1B flags]
0x03DD         COMMANDLOG      [1B subcommand][additional args...]
0x03DE-0x04FF                 Reserved for future server commands
```

---

## Module Commands (0xF000)

All module commands use opcode 0xF000 with a 4-byte module subcommand. The subcommand encodes module ID in the high 16 bits and command ID in the low 16 bits.

Frame format: [0xF000][2B mux_id][4B module_subcommand][payload...]

### JSON Module (Module ID: 0x0000)

All JSON commands use module subcommand 0x0000XXXX where XXXX is the command ID.

```
Subcommand     Command        Payload Format
=============  =============  =============================================================================
0x00000000     JSON.SET       [2B keylen][key][2B pathlen][path][4B jsonlen][json][1B flags]
0x00000001     JSON.GET       [2B keylen][key][2B numpaths]([2B pathlen][path])...
0x00000002     JSON.MGET      [2B numkeys]([2B keylen][key])...[2B pathlen][path]
0x00000003     JSON.MSET      [2B count]([2B keylen][key][2B pathlen][path][4B jsonlen][json])...
0x00000004     JSON.DEL       [2B keylen][key][2B pathlen?][path?]
0x00000005     JSON.FORGET    [2B keylen][key][2B pathlen?][path?]
0x00000006     JSON.TYPE      [2B keylen][key][2B pathlen?][path?]
0x00000007     JSON.CLEAR     [2B keylen][key][2B pathlen?][path?]
0x00000008     JSON.ARRAPPEND [2B keylen][key][2B pathlen][path][2B count]([4B jsonlen][json])...
0x00000009     JSON.ARRINDEX  [2B keylen][key][2B pathlen][path][4B jsonlen][json][8B start?][8B stop?]
0x0000000A     JSON.ARRINSERT [2B keylen][key][2B pathlen][path][8B index][2B count]([4B jsonlen][json])...
0x0000000B     JSON.ARRLEN    [2B keylen][key][2B pathlen?][path?]
0x0000000C     JSON.ARRPOP    [2B keylen][key][2B pathlen?][path?][8B index?]
0x0000000D     JSON.ARRTRIM   [2B keylen][key][2B pathlen][path][8B start][8B stop]
0x0000000E     JSON.OBJKEYS   [2B keylen][key][2B pathlen?][path?]
0x0000000F     JSON.OBJLEN    [2B keylen][key][2B pathlen?][path?]
0x00000010     JSON.STRLEN    [2B keylen][key][2B pathlen?][path?]
0x00000011     JSON.STRAPPEND [2B keylen][key][2B pathlen][path][4B jsonlen][json]
0x00000012     JSON.NUMINCRBY [2B keylen][key][2B pathlen][path][8B number]
0x00000013     JSON.NUMMULTBY [2B keylen][key][2B pathlen][path][8B number]
0x00000014     JSON.TOGGLE    [2B keylen][key][2B pathlen][path]
0x00000015     JSON.DEBUG     [1B subcommand][2B keylen][key][2B pathlen?][path?]
0x00000016     JSON.RESP      [2B keylen][key][2B pathlen?][path?]
0x00000017-0x0000FFFF         Reserved for future JSON commands
```

### Bloom Filter Module (Module ID: 0x0001)

All Bloom Filter commands use module subcommand 0x0001XXXX where XXXX is the command ID.

```
Subcommand     Command        Payload Format
=============  =============  =============================================================================
0x00010000     BF.ADD         [2B keylen][key][2B itemlen][item]
0x00010001     BF.MADD        [2B keylen][key][2B count]([2B itemlen][item])...
0x00010002     BF.EXISTS      [2B keylen][key][2B itemlen][item]
0x00010003     BF.MEXISTS     [2B keylen][key][2B count]([2B itemlen][item])...
0x00010004     BF.RESERVE     [2B keylen][key][8B error_rate][8B capacity][1B flags]
0x00010005     BF.INSERT      [2B keylen][key][1B flags][2B count]([2B itemlen][item])...
0x00010006     BF.CARD        [2B keylen][key]
0x00010007     BF.INFO        [2B keylen][key]
0x00010008     BF.LOAD        [2B keylen][key][4B datalen][data]
0x00010009-0x0001FFFF         Reserved for future Bloom Filter commands
```

### Search Module (Module ID: 0x0002)

All Search commands use module subcommand 0x0002XXXX where XXXX is the command ID.

```
Subcommand     Command        Payload Format
=============  =============  =============================================================================
0x00020000     FT.CREATE       [2B idxlen][index][...complex schema definition]
0x00020001     FT.SEARCH       [2B idxlen][index][2B querylen][query][...options]
0x00020002     FT.DROPINDEX    [2B idxlen][index][1B flags]
0x00020003     FT.INFO         [2B idxlen][index]
0x00020004     FT._LIST        
0x00020005-0x0002FFFF          Reserved for future Search commands
```

### Future Modules

Module IDs 0x0003 through 0xFFFF are reserved for future modules. Each module can define up to 65,536 commands using the 4-byte subcommand format.

---

## Control Operations (0xFF00 - 0xFFFE)

```
Opcode         Command        Payload Format
=============  =============  =============================================================================
0xFF00         MUX_CREATE     [2B mux_id]
0xFF01         MUX_CLOSE      [2B mux_id]
0xFF02-0xFFFE                 Reserved for control commands
```

## RESP Passthrough (0xFFFF)

```
Opcode         Command        Payload Format
=============  =============  =============================================================================
0xFFFF         RESP Passthrough [4B resp_length][RESP text data...]
```

The opcode 0xFFFF enables sending plain text RESP commands over a binary connection. The frame format is 8 bytes header: 2 bytes for 0xFFFF opcode, 2 bytes for mux ID, 4 bytes for RESP data length, followed by the raw RESP text command. The server parses the RESP data as if it arrived on a text connection. The response is returned in binary RESPB format.

---

## Response Opcodes (0x8000 - 0xFFFE)

Common response opcodes used across all commands:

```
Opcode         Type                   Payload Format
=============  =====================  =============================================================================
0x8000         Simple String (OK)     [2B len][string] or empty for "OK"
0x8001         Error                  [2B errlen][error_message]
0x8002         Integer                [8B int64]
0x8003         Bulk String            [4B len][data] or [0xFFFFFFFF] for null
0x8004         Array                  [2B count] followed by elements
0x8005         Null                   
0x8006         Boolean                [1B bool]
0x8007         Double                 [8B float64]
0x8008         Map                    [2B count] followed by key-value pairs
0x8009         Set                    [2B count] followed by elements
0x800A         Push (Pub/Sub)         [2B count] followed by message components
0x800B-0xFFFD                         Reserved for future response types
```

---

## Opcode Summary by Category

### Core Commands (0x0000 - 0xEFFF)

```
Category       Opcode Range      Count    Reserved
=============  ================  =======  ============
String         0x0000-0x003F     64       41 unused
List           0x0040-0x007F     64       42 unused
Set            0x0080-0x00BF     64       47 unused
Sorted Set     0x00C0-0x00FF     64       35 unused
Hash           0x0100-0x013F     64       37 unused
Bitmap         0x0140-0x015F     32       25 unused
HyperLogLog    0x0160-0x017F     32       27 unused
Geospatial     0x0180-0x01BF     64       54 unused
Stream         0x01C0-0x01FF     64       49 unused
Pub/Sub        0x0200-0x023F     64       55 unused
Transaction    0x0240-0x025F     32       27 unused
Scripting      0x0260-0x02BF     96       88 unused
Generic        0x02C0-0x02FF     64       34 unused
Connection     0x0300-0x033F     64       56 unused
Cluster        0x0340-0x03BF     128      124 unused
Server         0x03C0-0x04FF     320      289 unused
Reserved       0x0500-0xEFFF     57,856   All unused
```

### Module Commands (0xF000)

```
Module         Module ID    Commands Mapped    Commands Available
=============  ===========  =================  ===================
JSON           0x0000       24                 65,512 remaining
Bloom Filter   0x0001       9                  65,527 remaining
Search         0x0002       5                  65,531 remaining
Future Modules 0x0003-0xFFFF 0                 65,536 per module
```

### Special Opcodes

```
Opcode         Purpose            Notes
=============  =================  =============================================================================
0xFF00-0xFFFE  Control operations Reserved for protocol control
0xFFFF         RESP passthrough   Enables plain text RESP over binary connection
```

**Total Core Commands Mapped:** 432  
**Total Module Commands Mapped:** 38  
**Core Opcode Space:** 61,440 opcodes (0x0000-0xEFFF)  
**Module Command Space:** 4,294,967,296 combinations (65,536 modules × 65,536 commands)

---

## Usage Examples

### Example 1: GET Command

**Text RESP:**
```
*2\r\n$3\r\nGET\r\n$6\r\nmykey\r\n
```
(22 bytes)

**Binary RESPB:**
```
[0x0000][MuxID][0x0006]["mykey"]
```
(10 bytes - 55% smaller)

### Example 2: SET Command

**Text RESP:**
```
*3\r\n$3\r\nSET\r\n$6\r\nmykey\r\n$5\r\nhello\r\n
```
(39 bytes)

**Binary RESPB:**
```
[0x0001][MuxID][0x0006]["mykey"][0x00000005]["hello"][0x00][0x0000000000000000]
```
(26 bytes - 33% smaller)

### Example 3: ZADD Command

**Text RESP:**
```
*4\r\n$4\r\nZADD\r\n$5\r\nmyset\r\n$3\r\n1.5\r\n$3\r\nfoo\r\n
```
(50 bytes)

**Binary RESPB:**
```
[0x00C0][MuxID][0x0005]["myset"][0x00][0x0001][double(1.5)][0x0003]["foo"]
```
(27 bytes, 46% smaller)

### Example 4: JSON.SET Command

**Text RESP:**
```
*4\r\n$8\r\nJSON.SET\r\n$7\r\nprofile\r\n$5\r\n.name\r\n$12\r\n"John Doe"\r\n
```
(64 bytes)

**Binary RESPB:**
```
[0xF000][MuxID][0x00000000][0x0007]profile[0x0005].name[0x0000000C]"John Doe"[0x00]
```
(44 bytes, 31% smaller)

The frame uses opcode 0xF000 for module commands. The 4-byte subcommand 0x00000000 encodes module ID 0x0000 (JSON) in the high 16 bits and command ID 0x0000 (JSON.SET) in the low 16 bits.

### Example 5: RESP Passthrough

**Text RESP:**
```
*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n
```
(33 bytes)

**Binary RESPB with Passthrough:**
```
[0xFFFF][MuxID][0x00000021]*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n
```
(41 bytes total, 8 bytes header + 33 bytes RESP data)

The opcode 0xFFFF enables sending plain text RESP commands over a binary connection. The length field 0x00000021 is 33 bytes, the size of the RESP command.

---

## Implementation Notes

1. Endianness: All multi-byte integers are in network byte order (big-endian)
2. NULL values: Represented by length field of 0xFFFFFFFF (4 bytes) or 0xFFFF (2 bytes) depending on field size
3. Flags: Bitfields where each bit represents an option (NX=0x01, XX=0x02, GT=0x04, LT=0x08, etc.)
4. Variable arguments: Commands with variable arguments start with a count field
5. Optional arguments: Indicated by flag bits. Presence determines if optional fields follow
6. Mux ID: 0x0000 is the default/primary mux channel. Clients choose IDs 0x0001-0xFFFF for multiplexing
7. Module commands: Use opcode 0xF000 with 4-byte subcommand. Extract module ID from high 16 bits and command ID from low 16 bits
8. RESP passthrough: Use opcode 0xFFFF to send plain text RESP commands. Read 4-byte length field, then read that many bytes as RESP data

---

## Version History

- v1.0 (2025-01-15): Initial opcode allocation for 432 Valkey commands with power-of-2 padding
- v2.0 (2025-01-XX): Restructured module commands to use hierarchical 0xF000 opcode with 4-byte subcommands. Changed 0xFFFF from extension opcode to RESP passthrough. Moved JSON, Bloom Filter, and Search commands to module section.

