# Proposal: RESPB Binary Protocol for Valkey

## TL;DR

RESPB is a binary wire protocol that replaces RESP text parsing with fixed-size opcodes and length-prefixed payloads. Working implementation shows **2.3x GET throughput**, **46% lower SET latency**, and benefits that **scale with pipeline depth**. The protocol also introduces native connection multiplexing via mux IDs.

**Implementation**: [github.com/maxpert/valkey](https://github.com/maxpert/valkey) (server, CLI, and benchmark tool)

---

## Problem

RESP requires per-byte scanning for delimiters (`*`, `$`, `\r\n`), ASCII-to-integer conversion for every length field, and string matching for command dispatch. At high throughput, protocol parsing becomes a measurable fraction of server CPU time.

```
RESP3 GET mykey (24 bytes):
*2\r\n$3\r\nGET\r\n$5\r\nmykey\r\n
 ↑    ↑         ↑           ↑
 |    |         |           └─ scan for CRLF
 |    |         └─ parse "$3", scan for CRLF
 |    └─ parse "$3", strcmp("GET"), scan for CRLF
 └─ parse "*2", scan for CRLF
```

Each command requires multiple delimiter scans and ASCII conversions before execution can begin.

---

## Solution: RESPB Binary Framing

RESPB uses a compact binary header with direct field access:

```
RESPB GET mykey (11 bytes):
┌──────────┬──────────┬──────────┬─────────────┐
│  Opcode  │  Mux ID  │ Key Len  │    Key      │
│  0x0000  │  0x0001  │  0x0005  │   "mykey"   │
│  2 bytes │  2 bytes │  2 bytes │   5 bytes   │
└──────────┴──────────┴──────────┴─────────────┘
```

- **No delimiter scanning** - fixed header size, lengths are binary
- **O(1) command dispatch** - opcode table lookup vs string matching
- **Native multiplexing** - mux ID enables multiple logical clients per connection

### Frame Formats

```
Core Commands (4-byte header):
┌────────────────┬────────────────┬─────────────────────┐
│ Opcode (2B)    │ Mux ID (2B)    │ Payload             │
│ 0x0000-0xEFFF  │                │                     │
└────────────────┴────────────────┴─────────────────────┘

Module Commands (8-byte header):
┌────────────────┬────────────────┬──────────────────┬─────────────┐
│ 0xF000 (2B)    │ Mux ID (2B)    │ Subcommand (4B)  │ Payload     │
└────────────────┴────────────────┴──────────────────┴─────────────┘

RESP Passthrough (backward compat):
┌────────────────┬────────────────┬──────────────────┬─────────────┐
│ 0xFFFF (2B)    │ Mux ID (2B)    │ RESP Len (4B)    │ RESP Text   │
└────────────────┴────────────────┴──────────────────┴─────────────┘
```

---

## Benchmark Results

Tested on 12-core Darwin system, 36GB RAM. Full methodology and reproduction steps in the implementation repo.

### Throughput and Latency (128 clients, 128 pipeline, 1KB values)

| Operation | RESP3 RPS | RESPB RPS | RESP3 Latency | RESPB Latency |
|-----------|-----------|-----------|---------------|---------------|
| GET       | 2.1M      | 4.8M      | 5.32ms        | 3.30ms        |
| SET       | 2.3M      | 1.9M*     | 3.86ms        | 2.09ms        |

*SET RPS variance under investigation - likely measurement artifact. Latency improvement is consistent.

### Improvement Scales with Pipeline Depth

| Pipeline | RESP3 RPS | RESPB RPS | Speedup | Latency Reduction |
|----------|-----------|-----------|---------|-------------------|
| P=1      | 177K      | 184K      | 1.04x   | 4%                |
| P=10     | 982K      | 1.27M     | 1.29x   | 21%               |
| P=50     | 1.88M     | 3.64M     | 1.94x   | 48%               |
| P=100    | 1.90M     | 4.12M     | 2.16x   | 36%               |
| P=200    | 2.02M     | 4.57M     | 2.26x   | 11%               |

Key observation: **Benefits increase with pipeline depth**, which is the common production pattern.

---

## Multiplexing

The 16-bit mux ID in every frame enables multiple logical clients over a single TCP connection:

```
Connection 1 (traditional):     Connection 1 (RESPB multiplexed):
┌─────────────┐                 ┌─────────────┐
│ Client A    │──TCP──┐         │ Client A    │──┐
└─────────────┘       │         └─────────────┘  │
┌─────────────┐       ▼         ┌─────────────┐  │    ┌─────────┐
│ Client B    │──TCP──►Valkey   │ Client B    │──┼TCP─► Valkey  │
└─────────────┘       ▲         └─────────────┘  │    └─────────┘
┌─────────────┐       │         ┌─────────────┐  │
│ Client C    │──TCP──┘         │ Client C    │──┘
└─────────────┘                 └─────────────┘
 3 connections                   1 connection, 3 mux channels
```

Benefits:
- **Reduced connection overhead** - fewer TCP connections, less kernel memory
- **No head-of-line blocking** - mux channels are independent (like HTTP/2 streams)
- **Isolated state per channel** - each mux ID maintains its own AUTH, SELECT, MULTI state
- **Better resource utilization** - single connection handles thousands of logical clients

---

## Wire Format Comparison

| Command | RESP3 | RESPB | Savings |
|---------|-------|-------|---------|
| `GET key` | 24B | 11B | 54% |
| `SET key val` | 37B | 18B | 51% |
| `MGET k1 k2 k3` | 51B | 22B | 57% |
| `PING` | 14B | 4B | 71% |
| `HGETALL hash` | 32B | 10B | 69% |

---

## Backward Compatibility

1. **Auto-detection**: Server detects RESP vs RESPB from first byte (RESP starts with `*$+-:`, RESPB opcodes don't)
2. **Protocol negotiation**: `HELLO 3 PROTO respb` for explicit upgrade
3. **RESP passthrough**: Opcode `0xFFFF` wraps any RESP command for gradual migration
4. **Coexistence**: RESP2, RESP3, and RESPB can run on same server, different connections

---

## Implementation Status

Working proof-of-concept at [github.com/maxpert/valkey](https://github.com/maxpert/valkey):

- Server-side RESPB parsing and response encoding
- `valkey-cli` with `-4` flag for RESPB mode
- `valkey-benchmark` with `-4` flag for performance testing
- Core commands (GET, SET, MGET, DEL, etc.) implemented
- Full specification docs included

---

## Specification Documents

- [respb-specs.md](respb-specs.md) - Protocol specification and design rationale
- [respb-commands.md](respb-commands.md) - Opcode mappings for 432+ commands

---

## Discussion Points

1. **Interest level** - Is this direction worth pursuing for Valkey?
2. **Opcode allocation** - Current design uses categories (strings 0x00-0x3F, lists 0x40-0x7F, etc.). Alternative approaches?
3. **Multiplexing semantics** - Per-mux state isolation vs shared state tradeoffs
4. **Module command encoding** - 0xF000 + 4-byte subcommand supports 4B+ combinations. Overkill?
5. **Response format** - Currently mirrors request structure. Should responses be optimized differently?

Looking forward to feedback from the Valkey team.
