# Redis Binary Protocol (RESPB) - Draft Specification

## Related Documents

- [respb-commands.md](respb-commands.md): Complete opcode mappings and payload formats for all 432+ Valkey commands
- [protocol-bench/BENCHMARK.md](protocol-bench/BENCHMARK.md): Detailed performance benchmarks using Valkey's production RESP parser

## Introduction

This document proposes RESPB (Redis Binary Protocol), a new binary encoded protocol for Redis that aims to improve network and CPU efficiency while adding built in multiplexing capabilities. Redis's current text based RESP2/RESP3 protocol is simple and fast. It's even described as comparable in performance to a binary protocol. However, RESP is not fully optimized for minimal wire overhead, and it lacks native support for multiplexing multiple clients on a single connection.

By redesigning RESP with compact binary frames and multiplexing (similar to HTTP/2), we can make Redis communication more efficient and robust. The goals of RESPB include:

- **High Efficiency**: Minimize network bytes and parsing overhead. Use a simple binary encoding that is quick to encode/decode with low CPU cost and reduces message size.
- **Simplicity**: Keep the protocol format concise and easy to implement, so that a one page cheat sheet could describe it.
- **Multiplexing**: Allow multiple independent request/reply mux channels over one TCP connection with no head of line blocking between logical clients, eliminating the need for excessive connection pooling.
- **Upgrade Support**: Provide a clear handshake so that client and server can switch from RESP text to RESPB binary smoothly.
- **Complete Coverage and Extensibility**: Support encoding of all Redis commands and data types (including complex replies and Pub/Sub) and allow extensibility for new commands or features.

## Protocol Overview

### Handshake and Version Negotiation

RESPB uses an initial handshake to negotiate the protocol upgrade. The client initiates the connection in binary mode by sending a magic header as the first bytes, which the server uses to detect RESPB (as opposed to legacy RESP). The handshake format is:

- **Magic bytes**: 0xD3 0xC1 (two fixed bytes unlikely to appear at start of a RESP text stream)
- **Version byte**: 0x01 (protocol version number for RESPB)
- **Flags byte**: A bitfield reserved for future use or feature negotiation (set to 0x00 for now)

On connection, if the server reads the magic bytes 0xD3 0xC1, it recognizes a binary protocol handshake. The server responds with an acknowledgment frame in binary format (echoing the version or an OK status) to confirm the upgrade. At this point, both client and server switch to RESPB for all further messages. If the server does not support RESPB, it will either ignore or send a RESP error, and the client should fall back to RESP. As an alternative upgrade path, a client could issue a textual HELLO command to negotiate a new protocol, but the magic byte handshake is the primary method to autodetect binary mode.

### Message Framing and Format

Once in binary mode, all communication consists of binary frames. Each frame can be a request from client or a response from server. The general frame format is:

**Header**: A fixed size header containing:
- **Opcode (2 bytes)**: A 16-bit identifier of the command or response type
- **Mux ID (2 bytes)**: A 16-bit identifier for the logical client mux channel (used for multiplexing, acts similarly to an HTTP/2 stream ID for multiplexing). The client chooses a non zero ID for each logical session (or 0x0000 for the initial default session if needed)

**Payload**: The remaining bytes, whose structure depends on the opcode. The payload typically contains the command's arguments or the response data, encoded in binary.

All integers in the header and payload are in network byte order (big endian). The opcode space is organized into distinct ranges for core commands, module commands, and special functions. A 16-bit mux ID allows up to 65,535 concurrent logical clients per connection.

There is no separate length field for the entire frame. The frame is parsed according to the known structure of each opcode payload. Each Redis command has a known number and type of arguments. Each argument is length prefixed, so the frame end can be determined without a total length field.

**Frame headers vary by command type:**

Core commands use a 4-byte header:
- Opcode (2 bytes): Command identifier in range 0x0000 to 0xEFFF
- Mux ID (2 bytes): Logical client mux channel identifier

Module commands use an 8-byte header:
- Opcode (2 bytes): Always 0xF000 to indicate module command
- Mux ID (2 bytes): Logical client mux channel identifier
- Module subcommand (4 bytes): 32-bit identifier encoding module ID and command ID

RESP passthrough uses an 8-byte header:
- Opcode (2 bytes): Always 0xFFFF to indicate RESP passthrough
- Mux ID (2 bytes): Logical client mux channel identifier
- RESP length (4 bytes): Length of following RESP text data

Protocol is simple enough that in a cheatsheet it can be described like this:

```
================================================================================
RESPB (Redis Binary Protocol) - Request Format Summary
================================================================================

HANDSHAKE
---------
Client sends: [0xD3][0xC1][0x01][0x00]
              magic  magic  ver   flags
Server responds: Binary acknowledgment frame

FRAME FORMATS
--------------
Core Commands (4-byte header):
  [opcode:2B][mux_id:2B][payload...]
  Opcode range: 0x0000 - 0xEFFF

Module Commands (8-byte header):
  [0xF000:2B][mux_id:2B][module_subcmd:4B][payload...]
  Module subcommand: [module_id:16b][command_id:16b]

RESP Passthrough (8-byte header):
  [0xFFFF:2B][mux_id:2B][resp_length:4B][RESP_text_data...]

Response Opcodes:
  Range: 0x8000 - 0xFFFE
  0x8000 = Simple string (OK)
  0x8001 = Error
  0x8002 = Integer
  0x8003 = Bulk string
  0x8004 = Array
  0x8005 = Null

OPCODE SPACE MAP
-----------------
0x0000 - 0x003F  String operations      (GET, SET, MGET, MSET, etc.)
0x0040 - 0x007F  List operations        (LPUSH, RPUSH, LRANGE, etc.)
0x0080 - 0x00BF  Set operations          (SADD, SREM, SMEMBERS, etc.)
0x00C0 - 0x00FF  Sorted set operations   (ZADD, ZRANGE, ZSCORE, etc.)
0x0100 - 0x013F  Hash operations         (HSET, HGET, HGETALL, etc.)
0x0140 - 0x015F  Bitmap operations       (SETBIT, GETBIT, BITCOUNT, etc.)
0x0160 - 0x017F  HyperLogLog operations  (PFADD, PFCOUNT, etc.)
0x0180 - 0x01BF  Geospatial operations  (GEOADD, GEODIST, etc.)
0x01C0 - 0x01FF  Stream operations      (XADD, XREAD, XRANGE, etc.)
0x0200 - 0x023F  Pub/Sub operations     (PUBLISH, SUBSCRIBE, etc.)
0x0240 - 0x025F  Transaction operations (MULTI, EXEC, DISCARD, etc.)
0x0260 - 0x02BF  Scripting and functions (EVAL, EVALSHA, FCALL, etc.)
0x02C0 - 0x02FF  Generic key operations  (DEL, EXISTS, EXPIRE, etc.)
0x0300 - 0x033F  Connection management  (AUTH, PING, ECHO, etc.)
0x0340 - 0x03BF  Cluster management      (CLUSTER, ASKING, etc.)
0x03C0 - 0x04FF  Server management       (INFO, CONFIG, COMMAND, etc.)
0x0500 - 0xEFFF  Reserved for future core commands

0xF000           Module commands         (JSON.*, BF.*, FT.*, etc.)
0xF001 - 0xFFFE  Reserved for extensions
0xFFFF           RESP passthrough        (Backward compatibility)

ENCODING CONVENTIONS
--------------------
Length Prefixes:
  2-byte length: Keys, field names, small strings (max 65,535 bytes)
  4-byte length: Values, large payloads (max 4,294,967,295 bytes)

Numeric Types:
  8-byte signed int: Counters, increments, expiries (int64)
  8-byte float: Scores, coordinates (IEEE 754 binary64)

Flags:
  1-byte bitfield: Option flags (NX=0x01, XX=0x02, GT=0x04, LT=0x08)

Variable Arity:
  [2B count][element1][element2]...[elementN]
  Fixed arity: No count field, decoder knows from opcode

Byte Order:
  All multi-byte integers: Network byte order (big-endian)

COMMON COMMAND EXAMPLES
------------------------
GET key:
  Request:  [0x0000][mux_id][2B keylen][key]
  Response: [0x8003][mux_id][4B vallen][value] or [0x8005] for null

SET key value:
  Request:  [0x0001][mux_id][2B keylen][key][4B vallen][value][1B flags][8B expiry?]
  Response: [0x8000][mux_id] (OK)

MGET key1 key2 key3:
  Request:  [0x000C][mux_id][0x0003][2B len1][key1][2B len2][key2][2B len3][key3]
  Response: [0x8004][mux_id][0x0003][element1][element2][element3]

SADD key member1 member2:
  Request:  [0x0080][mux_id][2B keylen][key][0x0002][2B len1][mem1][2B len2][mem2]
  Response: [0x8002][mux_id][8B integer] (count of new members)

ZADD key score member:
  Request:  [0x00C0][mux_id][2B keylen][key][1B flags][0x0001][8B score][2B memlen][member]
  Response: [0x8002][mux_id][8B integer] (count of new elements)

HSET key field value:
  Request:  [0x0100][mux_id][2B keylen][key][0x0001][2B fieldlen][field][4B vallen][value]
  Response: [0x8002][mux_id][8B integer] (0 or 1)

JSON.SET key path value:
  Request:  [0xF000][mux_id][0x00000000][2B keylen][key][2B pathlen][path][4B vallen][value]
  Module ID 0x0000 = JSON, Command ID 0x0000 = JSON.SET

PING:
  Request:  [0x0300][mux_id]
  Response: [0x8000][mux_id][0x0002][OK]

MULTIPLEXING
------------
Mux ID: 16-bit identifier (0x0000 - 0xFFFF)
  - Each logical client session uses unique mux ID
  - Commands within mux channel execute in strict order
  - Responses from different mux channels can arrive out of order
  - Up to 65,535 concurrent mux channels per connection

NULL VALUES
-----------
Null bulk string: Length = 0xFFFF (2-byte) or 0xFFFFFFFF (4-byte)
Null array: Count = 0xFFFF
Or use response opcode 0x8005

ERRORS
------
Error response: [0x8001][mux_id][2B msglen][error_message]
No leading minus or trailing CRLF, opcode indicates error

================================================================================
```

**Request Frame Structure**: For fixed arity commands (commands with a fixed number of arguments), the payload encodes each argument as length plus data without an explicit count of arguments. The decoder knows how many arguments to expect from the opcode. For variable arity commands (commands taking a list of keys or fields), the payload begins with a 2-byte count of how many elements follow, then each element is encoded as length plus data. This count allows the receiver to loop through the correct number of arguments. The 2-byte count can represent up to 65,535 elements in one message, more than enough for multi key operations.

**Response Frame Structure**: Responses use the same framing with a 2-byte opcode (distinct ranges are used for responses to differentiate them from requests) and the same 2-byte mux ID to indicate which request or client mux channel the response belongs to. The payload then encodes the response data. Simple responses (like an integer or status) may fit entirely in the payload. Complex responses (like arrays of values) are encoded similarly to requests. An array response uses a count followed by each element's data (with type tags if needed for heterogeneous types). In general:

- **Simple string OK**: Encoded as a short response opcode (0x0100 for OK) with no further payload or with a length prefixed message OK
- **Error**: Encoded with an error opcode plus an error code or message string in the payload
- **Integer**: Encoded either in a fixed 8-byte field or as a smaller integer field (8 bytes for 64-bit integer)
- **Bulk string**: Encoded as length plus bytes similar to request bulk data
- **Array (multibulk)**: Encoded as a count followed by that many sub elements encoded in sequence. Sub elements can be typed. An element could itself be a bulk string, integer, etc. A type byte or dedicated opcodes for nested types can be used, but for simplicity we can treat composite responses as their own opcodes or as a structured payload with type tags.

As a cheatsheat for respone format it can be described as following.

```
================================================================================
RESPB (Redis Binary Protocol) - Response Format Summary
================================================================================

RESPONSE FRAME FORMATS
----------------------
Core Responses (4-byte header):
  [opcode:2B][mux_id:2B][payload...]

Module Responses (8-byte header):
  [opcode:2B][mux_id:2B][module_subcmd:4B][payload...]

RESP Passthrough Responses (8-byte header):
  [0xFFFF][mux_id:2B][resp_len:4B][resp_data...]

--------------------------------------------------------------------------------
TYPICAL RESPONSE PAYLOADS
-------------------------
Simple OK:
  [0x8000][mux_id][0x0002][OK]
  Example: OK/status reply

Error:
  [0x8001][mux_id][2B msglen][error_message]
  Example: Error with message

Integer:
  [0x8002][mux_id][8B integer]
  Example: Returns integer value (e.g., INCR, ZADD count)

Bulk String:
  [0x8003][mux_id][4B len][data]
  Example: Single value, e.g., GET result

Null Bulk/String:
  [0x8005][mux_id]           (explicit null)
  Or use length = 0xFFFF or 0xFFFFFFFF for bulk string

Array:
  [0x8004][mux_id][2B count][elem1][elem2]...
  Each element: [type][length][data], repeated

Null Array:
  [count = 0xFFFF] or opcode [0x8005]

--------------------------------------------------------------------------------
RESPONSE OPCODE QUICK REF
-------------------------
0x8000 : OK/status reply
0x8001 : Error
0x8002 : Integer
0x8003 : Bulk string
0x8004 : Array (multi-bulk reply)
0x8005 : Null (for bulk, array, or other types)
0xFFFF : RESP passthrough

--------------------------------------------------------------------------------
RESPONSE DECODING EXAMPLES
--------------------------
- GET key -> [0x8003][mux_id][4B len][data] (bulk string or 0x8005 for null)
- ZCARD key -> [0x8002][mux_id][8B integer]
- MGET key1 key2 -> [0x8004][mux_id][2B count][bulk_or_null]...
- ERROR reply -> [0x8001][mux_id][2B msglen][error_message]

All responses use mux_id to match original request mux channel.
================================================================================
```

Every response is correlated to a request by the mux ID. The client sets the mux ID in the request. The server copies it into the response header. This enables out of order responses when multiplexing. The client matches replies to requests using mux IDs. Within a given mux channel, Redis preserves command ordering semantics. If a single logical client issues multiple pipelined requests, their responses come in order for that mux channel. Different mux channels operate independently in parallel. A blocking command on one mux channel does not block command processing on other mux channels. This achieves true concurrent multiplexing.

### Opcode Space Map

The RESPB opcode space is divided into distinct ranges for different command types. This organization enables efficient dispatch and future expansion.

**Core Commands: 0x0000 to 0xEFFF**

Core commands use a 4-byte header. The opcode directly identifies the command. The opcode space is organized by command category with ranges allocated for string operations, lists, sets, sorted sets, hashes, bitmaps, geospatial, streams, pub/sub, transactions, scripting, and server management.

**Module Commands: 0xF000**

Module commands use an 8-byte header. The header includes the 0xF000 opcode, mux ID, and a 4-byte module subcommand. The subcommand encodes module ID in the high 16 bits and command ID in the low 16 bits. This design supports up to 65,536 modules, each with up to 65,536 commands.

**RESP Passthrough: 0xFFFF**

The opcode 0xFFFF enables sending plain text RESP commands over a binary connection for backward compatibility and debugging. Frame format is [0xFFFF][mux_id][4B RESP_length][RESP_text_data].

**Reserved Range: 0xF001 to 0xFFFE**

This range is reserved for future protocol extensions.

For complete opcode mappings, command payload formats, and all 432+ Valkey commands, see [respb-commands.md](respb-commands.md).

### Multiplexing and Mux Management

By including the mux ID in every frame, RESPB natively supports multiplexing many logical clients over one connection. This design is inspired by HTTP/2 multiplexing and eliminates the need for connection pooling. Clients can create multiple logical sessions (mux channels) that the server will handle in parallel on the backend event loop. From the server's perspective, each mux channel behaves like an isolated client. It maintains its own state (authentication, selected database, transaction context, Pub/Sub subscriptions) and command semantics, just as separate TCP connections would. For example, one mux channel can be in a transaction (MULTI) or blocked on a BLPOP, while other mux channels on the same connection continue to execute commands without interference.

Mux Management: Mux IDs are client chosen (0x0000 to 0xFFFF). The first mux channel (typically 0x0000 or 0x0001) is implicitly active after handshake. To create additional mux channels, the client uses a new mux ID in any command frame. The server allocates mux state lazily when it receives the first command on that mux ID. If the server cannot allocate resources for a new mux channel, such as when the connection has reached its mux limit, it returns an error response on that mux channel. Mux channels can be closed explicitly via a Close Mux control message (opcode 0xFFFE) or implicitly when the connection closes. The server may also close idle mux channels after a timeout period.

This approach treats multiplexed sessions much like independent clients. Using such a protocol achieves similar performance to pipeline mode while maintaining the same functionality and semantics as if each client was on a dedicated connection. The overhead of extra TCP connections is avoided, and fairness can be managed by the server so one mux channel's large reply doesn't starve others (the server can intermix reads/writes from multiple mux channels in its event loop). The result is better resource utilization and simpler client resource management (one TCP connection for many logical clients).

### Data Types and Encoding

RESPB is binary safe and encodes all data as length prefixed byte sequences or fixed size binary fields. It eliminates all CRLF delimiters and textual markers from the wire format, reducing overhead. Here's how fundamental RESP types are represented:

**Commands and Bulk Strings**: Every command and its string arguments are sent as raw byte arrays with a preceding length. For example, a key or value is encoded as 2-byte length plus raw bytes if its length fits in 16 bits. For larger bulk data (big values), a 4-byte length field is used. Specifically, each opcode defines the size of length fields for each argument:

- For **keys, small strings, and typical arguments**: A 2-byte length prefix is used (up to 65,535 bytes). This covers most use cases since keys are usually small.
- For **large bulk data (values, payloads)**: A 4-byte length prefix is used when an argument can be larger than 64KB. For instance, the SET command uses a 4-byte length for the value field, allowing values up to 2^32-1 bytes.
- Numeric values (increments, expiries) are encoded in binary numeric form (8-byte signed integers for counters, 8-byte IEEE 754 for floating point scores). This is both compact and avoids parsing ASCII digits. For example, a ZADD score is an 8-byte float instead of a string of decimal characters.

**Integers (Response)**: Integer replies (like the result of INCR or the number of elements from SCARD) are sent as 8-byte signed integers in the payload, rather than as a string with a colon prefix. This fixed size covers the Redis 64-bit integer range.

**Simple Strings and Errors**: These are usually short. In RESPB, they are indicated by distinct opcodes:

- A successful OK reply (for commands like SET) can be indicated by a special response opcode (for instance, 0x0100) with no payload (meaning OK), or with a small payload length plus OK. Either approach avoids the plus OK CRLF overhead of text.
- Error messages are sent with an error opcode (0x0101) and then a length prefixed error string. There's no leading minus or trailing CRLF. The opcode itself denotes an error. The error format could include a sub code or just the message.

**Arrays (Multi bulk replies)**: Arrays are encoded as a 2-byte count of elements, followed by each element in sequence. Each element can be a bulk string (with its own length prefix), an integer (with a fixed 8-byte value), or even a nested array. In case of nested arrays or mixed type arrays (as in RESP3), a type byte can precede each element to indicate its type (0x01 equals bulk string, 0x02 equals integer, 0x03 equals null, 0x04 equals array start). The protocol can define separate opcodes for structured responses. One simple method is to use distinct response opcodes for composite types (an opcode meaning array of bulk strings follows) to avoid per element type tags. However, for full generality, including a type marker for each element allows encoding RESP3's richer replies (maps, sets, nulls, boolean, double). This draft will not delve into every RESP3 type, but the extensibility principle ensures we can add these encodings.

**Null Bulk/Null Array**: A null bulk string (RESP dollar minus 1) can be represented by a length of 0xFFFF (65535) as a special indicator for null (since a real bulk string can't have that length in a 2-byte scheme) or by a reserved opcode in the response. Similarly, a null array could be indicated by count equals 0xFFFF. Alternate approach includes a type byte for Null before an element without any data. The exact marker can be decided for clarity, but it will be unambiguous in context.

### Command Encoding Examples

Below are examples showing how to encode common Redis commands in RESPB. We use this notation: `OP` is the 2-byte opcode in hex. Fields in brackets are length-prefixed data.

`GET key`:
- Request: `OP=0x0000`, `[2-byte key length][key bytes]`. For `GET mykey` (5 bytes), the frame is `0x0000`, MuxID, `0x0005`, `mykey`.
- Response: `OP=0x8003` (bulk string) with `[4B len][value]`, or `0x8005` for null.

`SET key value`:
- Request: `OP=0x0001`, `[2B keylen][key] [4B vallen][value] [1B flags] [8B expiry?]`. Flags: bit 0 = NX, bit 1 = XX. Example: `SET mykey hello` with NX and EX=60 is `0x0001`, MuxID, `[0x0005][mykey] [0x0005][hello] [0x05] [0x0000003C]`.
- Response: `OP=0x8000` (OK) on success.

`MGET key1 key2 keyN`:
- Request: `OP=0x000C`, `[2B count=N] [2B len(key1)][key1] ... [2B len(keyN)][keyN]`.
- Response: `OP=0x8004` (array) with `[2B count]` followed by elements.

`JSON.SET key path value`:
- Request: `OP=0xF000`, MuxID, `[0x00000000]` (module 0x0000, command 0x0000), `[2B keylen][key] [2B pathlen][path] [4B jsonlen][json] [1B flags]`.
- Response: `OP=0x8000` (OK) on success.

See [respb-commands.md](respb-commands.md) for complete opcode mappings and payload formats for all 432+ commands.

### Extensibility and Fallback Mechanism

RESPB supports new commands through three mechanisms.

Core opcode assignment: New core commands receive opcodes in the 0x0000 to 0xEFFF range. The server dispatches these commands directly using the opcode value. This provides the best performance.

Module commands: Use opcode 0xF000 with a 4-byte subcommand. The subcommand encodes module ID in the high 16 bits and command ID in the low 16 bits. This design supports over 4 billion module command combinations.

RESP passthrough: Use opcode 0xFFFF to send plain text RESP commands over a binary connection. Frame format is [0xFFFF][mux_id][4B RESP_length][RESP_text_data]. The server parses the RESP data as if it arrived on a text connection. The response returns in binary RESPB format. Use passthrough for commands not yet assigned opcodes, debugging, clients without binary support, or gradual migration.

For unknown opcodes outside these ranges, the server returns an error. The client can retry using RESP passthrough as a fallback.

## Efficiency Analysis

One of the primary motivations for RESPB is to improve efficiency in network bandwidth and CPU usage compared to the textual RESP protocol. This section provides a high-level overview. For detailed performance benchmarks using Valkey's production parser, see [protocol-bench/BENCHMARK.md](protocol-bench/BENCHMARK.md).

### Network Overhead Comparison

RESP (text protocol) already minimizes unnecessary bytes relative to HTTP or other protocols, but it still carries type characters, CRLF delimiters, and ASCII representations of lengths and numbers. RESPB eliminates or compresses all of these:

- **Command names:** In text, each command verb (e.g. `LRANGE`, `HSET`) is sent in full. In binary, command names are replaced by a 2-byte opcode. For instance, `GET` (3 bytes + overhead) becomes `0x0001`. A longer command like `ZRANGEBYSCORE` (13 bytes) also becomes just 2 bytes. This can significantly cut down request size, especially for commands with long names or small arguments.

- **Array and bulk formatting:** Each RESP request has a `*<count>\r\n` at the start and a `$<len>\r\n` before every argument, plus an extra `\r\n` after. This adds about 5 bytes overhead per argument (not including the digit count), and about 3–4 bytes for the command count and command name length lines. In RESPB, the overhead per argument is just the binary length field (2 or 4 bytes, with no CRLF). This typically saves around 3 bytes per argument. The initial `*<count>\r\n` is replaced by either an implicit count (for fixed arity) or a 2-byte count (for variable arity), saving another 2–3 bytes.

- **Numeric parameters:** In text, numbers are sent as decimal ASCII (e.g., `SETEX key 100` sends `"100"` as 3 bytes plus framing). In binary, numeric parameters are transmitted as binary integers (1 to 8 bytes). For small integers like 100, you might use 4 bytes (`0x00000064`) instead of `"100"` (3 bytes). The raw size may be slightly larger, but binary eliminates the need for `$3\r\n...\r\n` around it (which would total 7 bytes for that number). Overall, even small numbers have no extra overhead beyond their fixed binary size. Large numbers benefit even more (`4294967296` as ASCII is 10 bytes plus overhead, but as 8-byte binary it is just 8 bytes total).

- **Response framing:** The same type of improvement applies to replies. A multi-bulk reply in text uses the `*<count>\r\n` and `$<len>\r\n` wrappers for each element. In binary, we use a count and length prefixes. An integer reply in text (like `:12345\r\n`, 7 bytes) becomes an 8-byte binary value (slightly larger for small numbers, but requires no parsing). An error (`-ERR something\r\n`) becomes an opcode plus the message, without the `-` and `\r\n`. Note: Some replies (such as very small integers or `"OK"`) may be a few bytes larger in binary, but these increases are usually negligible and the CPU cost is reduced. Most replies, especially those with multiple items, are smaller or comparable in size.

To concretely illustrate the network savings, we can compare the byte-size of example messages in RESP vs RESPB. Below, we use a Python script to construct some common commands in both protocols and measure their lengths:

```python
# Define helper functions to build RESP text command and RESPB binary command
def resp_text_command(parts):
    # parts: list of byte strings or strings for the command and its arguments
    out = b"*" + str(len(parts)).encode() + b"\r\n"
    for p in parts:
        if isinstance(p, str):
            p = p.encode()
        out += b"$" + str(len(p)).encode() + b"\r\n" + p + b"\r\n"
    return out

# Binary encoding builder functions for a few example commands (following the spec above):
import struct
def bin_frame(opcode, mux_id):
    return opcode.to_bytes(2,'big') + mux_id.to_bytes(2,'big')

def bin_get(key):
    return bin_frame(0x0001, 0) + len(key).to_bytes(2,'big') + key

def bin_set(key, value):
    # opcode 0x0002 for SET, 2B key len + key, 4B value len + value
    return bin_frame(0x0002, 0) + len(key).to_bytes(2,'big') + key + len(value).to_bytes(4,'big') + value

def bin_mget(keys):
    frame = bin_frame(0x0003, 0) + len(keys).to_bytes(2,'big')
    for k in keys:
        frame += len(k).to_bytes(2,'big') + k
    return frame

def bin_mset(pairs):
    frame = bin_frame(0x0004, 0) + len(pairs).to_bytes(2,'big')
    for k,v in pairs:
        frame += len(k).to_bytes(2,'big') + k + len(v).to_bytes(4,'big') + v
    return frame

# Now let's compare sizes for various scenarios:
key = b"foo" 
val = b"hello"
print("GET foo - RESP:", len(resp_text_command(["GET", key])), "bytes; RESPB:", len(bin_get(key)), "bytes")
print("SET foo hello - RESP:", len(resp_text_command(["SET", key, val])), "bytes; RESPB:", len(bin_set(key, val)), "bytes")

# Multi-key operations:
keys = [b"k1", b"k2", b"k3"]
print("MGET 3 keys - RESP:", len(resp_text_command(["MGET"]+keys)), "bytes; RESPB:", len(bin_mget(keys)), "bytes")
pairs = [(b"k1", b"1"), (b"k2", b"2"), (b"k3", b"3")]
print("MSET 3 pairs - RESP:", len(resp_text_command(["MSET"] + [item for pair in pairs for item in pair])), 
      "bytes; RESPB:", len(bin_mset(pairs)), "bytes")
```

When run, this script produces output illustrating the byte counts in each protocol:

```
GET foo - RESP: 22 bytes; RESPB: 9 bytes  
SET foo hello - RESP: 35 bytes; RESPB: 18 bytes  
MGET 3 keys - RESP: 54 bytes; RESPB: 30 bytes  
MSET 3 pairs - RESP: 76 bytes; RESPB: 44 bytes  
```

We can break down these numbers to see where the savings come from:

-   GET "foo": Text version `*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n` is 22 bytes. Binary is 9 bytes (`[0x0001][0x0000][0x0003]"foo"`). Eliminating the command name and CRLFs cuts the size to about 40%. The text format has overhead from `*2\r\n` (4 bytes), `$3\r\n` (for GET), the word GET itself, another `$3\r\n` for the key, and final `\r\n`. Binary has just a 4-byte header and a 2-byte length for the key.
    
-   SET "foo" "hello": Text `*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$5\r\nhello\r\n` is 35 bytes. Binary is 18 bytes (`OP=0x0002,ID=0,[len=3]"foo",[len=5]"hello"`). The savings come from condensing SET to 2 bytes and removing 3 `$..CRLF` sequences. Even with a 4-byte length for the value in binary, the result is roughly half the size.
    
-   MGET 3 keys: Text requires `*4\r\n` + `$4\r\nMGET\r\n` + three `$2\r\nk1\r\n`, etc., totaling 54 bytes for 3 short keys. Binary is 30 bytes for 3 keys. As the number of keys grows, text overhead grows faster than binary. For example, with 8 keys, text was 94 bytes vs binary 54 bytes in our tests. This trend continues linearly. Binary saves about 40-45% in these multi-key scenarios.
    
-   MSET 3 pairs: Text `*7` (because MSET plus 3 keys and 3 vals equals 7 array elements) totals 76 bytes for small keys/values. Binary is 44 bytes. Binary is about 42% smaller. The more keys/pairs involved, the more bytes saved. Each additional pair in text adds at least 4 extra framing bytes (two `$..CRLF` wrappers) in addition to the key and value themselves. Binary adds just 2+4 bytes of length prefixes.

Other commands show similar improvements:

- SADD with multiple members: For example, `SADD myset x y z` (key "myset", 3 members). Text encoding is 56 bytes. Binary is 32 bytes (about 43% reduction). Each additional member adds about 7 bytes in text (including the `$` length lines) vs 2 bytes in binary.

- ZADD with multiple scores: Binary's 8-byte score fields add some overhead compared to sending small integers as text. It saves the cost of parsing and allows precise transmission of floating-point values. For instance, `ZADD z 1.0 a 2.0 b 3.0 c`: text about 92 bytes vs binary about 65 bytes (about 30% smaller). If scores have many digits, binary would win by even more.

- PUBLISH small message vs large message: We measured an example `PUBLISH news hello` equals 38 bytes text vs 19 bytes binary (50% saving for a small message). For a larger message of 100 bytes, text was 135 bytes vs 114 bytes binary (about 15% saving). The overhead difference is constant (about 21 bytes saved here). The percentage benefit is bigger for small payloads. In all cases, binary is never larger than text for requests. It is always more compact or equal.

It's worth noting that as the data payload grows very large (e.g. a 1 MB value), the protocol overhead in either scheme becomes negligible (<0.01%). The binary protocol's main win is in reducing overhead for _small to medium_ payloads and command-rich interactions, which are common in real workloads (many small keys/values, many commands per second). It also consistently saves a few bytes on every single command, which adds up in high-throughput scenarios.

### CPU and Parsing Efficiency

RESPB is designed to be simpler and faster to parse than RESP:

- Direct length reading: Fixed headers and explicit length prefixes eliminate delimiter scanning and ASCII-to-integer conversion
- Fewer syscalls: Tighter packing means more commands per TCP packet
- O(1) command dispatch: Opcodes enable direct table lookup instead of string matching
- Binary numeric parsing: No text conversion for integers and floats
- Multiplexing benefits: Fewer connections reduce kernel overhead and context switches

For detailed performance benchmarks using Valkey's production RESP parser, see [protocol-bench/BENCHMARK.md](protocol-bench/BENCHMARK.md). The benchmarks show 33.5x average CPU speedup, 24.9% bandwidth savings, and 59.4% memory savings.

### Bandwidth and Throughput Impact

Combining the network and CPU considerations, RESPB can significantly improve performance in high-throughput environments:

- Bandwidth savings: 30-60% reduction for request-heavy loads with many small commands
- Lower latency: Fewer bytes and CPU cycles per command reduce end-to-end latency
- Reduced connection overhead: Multiplexing allows many logical clients on one TCP connection
- Better fairness: Long-running operations on one mux channel do not block network processing of others

For detailed throughput measurements and workload-specific results, see [protocol-bench/BENCHMARK.md](protocol-bench/BENCHMARK.md).

### AOF and Replication Efficiency Gains

Beyond real-time command processing, RESPB provides significant benefits for persistence and replication scenarios. Redis uses the RESP protocol format in two critical areas: AOF (Append Only File) persistence and master-to-replica replication. Both of these write every command to disk or transmit it over the network, making protocol efficiency directly impact storage costs and replication bandwidth.

**AOF File Size Reduction**

When Redis persists commands to disk using AOF, each command is written in RESP text format. Over time, these files can grow to gigabytes or terabytes depending on write volume. By using RESPB format for AOF files, databases can achieve immediate storage savings with no loss of data or functionality.

Real-world measurement from a production-scale dataset demonstrates these savings:

```
Dataset: Mendeley Twitter data (3.1 million commands)
Commands: SET operations with geo-tagged tweets and user data
Average key length: ~26 bytes (e.g., "real:geo:1020770485687275522")
Average value length: ~70 bytes (tweet text, coordinates, user data)

Results:
- RESP AOF size:    307,032,950 bytes (292.81 MB)
- RESPB AOF size:   281,985,806 bytes (268.92 MB)
- Savings:          25,047,144 bytes (8.2%)
- Per command:      8.0 bytes saved per command

Processing speed: 85,555 commands/second
Conversion time:  36.5 seconds for 3.1M commands
```

This 8.2% reduction translates directly to:

- **Disk space savings**: An AOF file that would normally consume 1 TB now requires only 918 GB, saving 82 GB of storage
- **Faster backup operations**: Smaller files mean faster backup transfers and reduced backup storage costs
- **Improved I/O performance**: Less data to write means lower disk I/O load, which can increase write throughput
- **Reduced recovery time**: Smaller AOF files load faster during Redis restarts, reducing downtime

It's important to note that the percentage savings varies based on data characteristics:

- **Higher savings (30-60%)**: Workloads with many small keys and values (e.g., counters, flags, short strings) see the greatest benefit because RESP protocol overhead is proportionally larger
- **Moderate savings (8-15%)**: Workloads with medium-sized data (like the Mendeley dataset above with ~100-byte average command size) still achieve meaningful reductions
- **Lower savings (1-5%)**: Workloads dominated by very large values (megabytes per command) see smaller percentage gains since the protocol overhead becomes negligible compared to payload size

The absolute savings remain consistent (approximately 8-20 bytes per command depending on argument count), but the percentage depends on payload size. For high-throughput systems processing billions of commands, even an 8% reduction represents substantial cost savings in storage infrastructure.

**Replication Bandwidth Reduction**

Redis replication works by forwarding every write command from master to replicas using the RESP protocol. In deployments with multiple replicas or cross-datacenter replication, network bandwidth becomes a critical resource. RESPB reduces this bandwidth requirement proportionally to the AOF savings.

For the example dataset above, the savings translate to replication scenarios:

```
Scenario: Redis master with 3 replicas
Write rate: 10,000 commands/second
Average command size: 98.4 bytes (RESP) vs 90.4 bytes (RESPB)

Network bandwidth per replica:
- RESP:  960.9 KB/s (98.4 bytes × 10,000 cmds)
- RESPB: 882.8 KB/s (90.4 bytes × 10,000 cmds)
- Saved: 78.1 KB/s per replica

Per replica savings:
- Per hour: 274.7 MB/hour
- Per day: 6.44 GB/day
- Per year: 2.29 TB/year

Total across 3 replicas: 234.4 KB/s (824 MB/hour, 19.3 GB/day, 6.88 TB/year)
```

Benefits for cross-datacenter replication:

- **Lower network costs**: Cloud providers charge for data transfer between regions. Reducing replication traffic by 8-15% directly reduces these costs
- **Reduced replication lag**: Less data to transmit means replicas stay more closely synchronized with the master, improving read consistency
- **Better bandwidth utilization**: In bandwidth-constrained environments, the saved capacity can be used for other services
- **TLS overhead reduction**: When replication uses TLS encryption, smaller payloads mean less encryption overhead and CPU usage

**Compound Benefits at Scale**

The efficiency gains compound when considering both persistence and replication together. A large-scale Redis deployment might have:

- Master with AOF enabled (saves 8.2% disk space and I/O)
- 3 replicas also with AOF enabled (each saves 8.2% disk space)
- Continuous replication traffic (saves 8.2% bandwidth per replica)
- Periodic AOF rewrites (faster due to smaller files)
- Backup operations (smaller files transfer faster and cost less to store)

For a deployment processing 1 billion commands per day with similar characteristics to the Mendeley dataset:

```
Daily volumes:
- Commands: 1,000,000,000
- RESP data: 91.64 GB
- RESPB data: 84.19 GB
- Daily savings: 7.45 GB

Annual impact (365 days):
- Storage saved: 2.66 TB per AOF file
- With 4 nodes (1 master + 3 replicas): 10.62 TB saved annually
- Replication traffic saved: 2.66 TB per replica annually
- Total replication savings: 7.97 TB across 3 replicas
```

These are conservative estimates based on real production data. Workloads with smaller average command sizes would see proportionally larger benefits.

**Backward Compatibility for Existing AOF Files**

Migrating from RESP to RESPB for persistence is straightforward:

1. Existing RESP AOF files remain readable (Redis continues supporting RESP)
2. Enable RESPB for new writes via configuration
3. Trigger AOF rewrite to convert existing data to RESPB format
4. Future writes automatically use RESPB, accumulating savings over time

The conversion process is efficient (demonstrated above at 85,000+ commands/second) and can be performed during maintenance windows or incrementally through standard AOF rewrite operations.


### Considerations and Trade-offs

It's important to note some trade-offs and how we mitigate them:

- **Readability:** The human-readability of RESP is lost in RESPB. However, this is usually not a concern in production (tools can decode binary frames if needed for debugging). The efficiency gains are considered worth this loss. Developers can always use RESP for manual debugging or via a flag since Redis can support both protocols on different connections.

- **Backward compatibility:** Old Redis servers won't understand RESPB. The handshake mechanism ensures they will likely reply with an error or close the connection, so clients can detect that and revert to RESP. New servers will support both RESP and RESPB (perhaps configurable or automatic). The introduction of a HELLO handshake in Redis 6+ suggests a path for protocol negotiation. Our approach with magic bytes is even more immediate but requires client cooperation.

- **Complexity of implementation:** While binary protocols can be trickier to implement correctly, we've kept the format as simple as possible:
  - Fixed header and predictable structure per opcode (no deeply nested variable lengths beyond what's absolutely needed).
  - No checksums or compression at this layer (those could be handled at a different layer if desired).
  - Many design elements mirror what Redis already does internally (e.g. it already handles length-prefixed strings in C code. Now it will just get them in binary directly).
  - The multiplexing part requires changes to manage multiple logical clients on one FD, but this is an extension of the existing client state handling (similar to how the server would handle many FDs, here it's many mux IDs on one FD).

- **Memory usage:** Because lengths are known upfront, the server can allocate exact buffers for arguments rather than use incremental reads or resizable buffers while parsing. This could potentially reduce memory overhead and fragmentation. However, maintaining multiple pending replies for multiplexed mux channels might use more memory if many mux channels are active (since each can have output buffers). The server will need to enforce per-mux output buffer limits, akin to the per-connection output buffer limits today, to avoid any one connection overwhelming memory by multiplexing too many large responses .

## Conclusion

The RESPB protocol achieves the objectives of efficiency and modern features for Redis communication. By switching to a binary encoding with opcodes and length-prefixing, we reduce network overhead (particularly for small commands) and simplify parsing. The introduction of multiplexed mux channels on a single connection brings Redis's client handling up to par with approaches used in HTTP/2, allowing **many logical clients on one connection with isolated contexts and no head-of-line blocking** . This will greatly reduce connection management burden and improve fairness and throughput in environments with many clients or microservices.

We also ensured that this design remains extensible. New commands or data types can be added without breaking the protocol. Core commands receive opcodes in the 0x0000 to 0xEFFF range. Module commands use the 0xF000 opcode with hierarchical subcommands. Unknown commands can use RESP passthrough via opcode 0xFFFF. The protocol is designed to be simple. A developer can implement it by following a straightforward state machine: read header, dispatch on opcode, read the known sequence of lengths and data, and process.

RESPB offers a path to boost Redis performance by cutting down bytes on the wire and CPU cycles per operation. It enables richer usage patterns (multiplexing) without complex client-side workarounds. It maintains backward compatibility (through a handshake and optional fallback to text) and aligns with Redis's ethos of high performance and simplicity.

For detailed performance benchmarks and implementation details, see [protocol-bench/BENCHMARK.md](protocol-bench/BENCHMARK.md). For complete command opcode mappings, see [respb-commands.md](respb-commands.md).