#!/usr/bin/env python3
"""
Generate binary workload files for the protocol benchmark
Converts between RESP and RESPB formats
"""

import sys
import struct
import argparse
from pathlib import Path

# RESPB opcodes (from respb-commands.md)
RESPB_OPCODES = {
    # String Operations (0x0000-0x003F)
    'GET': 0x0000,
    'SET': 0x0001,
    'APPEND': 0x0002,
    'DECR': 0x0003,
    'DECRBY': 0x0004,
    'GETDEL': 0x0005,
    'GETEX': 0x0006,
    'GETRANGE': 0x0007,
    'GETSET': 0x0008,
    'INCR': 0x0009,
    'INCRBY': 0x000A,
    'INCRBYFLOAT': 0x000B,
    'MGET': 0x000C,
    'MSET': 0x000D,
    'MSETNX': 0x000E,
    'PSETEX': 0x000F,
    'SETEX': 0x0010,
    'SETNX': 0x0011,
    'SETRANGE': 0x0012,
    'STRLEN': 0x0013,
    'SUBSTR': 0x0014,
    'LCS': 0x0015,
    'DELIFEQ': 0x0016,
    
    # List Operations (0x0040-0x007F)
    'LPUSH': 0x0040,
    'RPUSH': 0x0041,
    'LPOP': 0x0042,
    'RPOP': 0x0043,
    'LLEN': 0x0044,
    'LRANGE': 0x0045,
    'LINDEX': 0x0046,
    'LSET': 0x0047,
    'LREM': 0x0048,
    'LTRIM': 0x0049,
    'LINSERT': 0x004A,
    'LPUSHX': 0x004B,
    'RPUSHX': 0x004C,
    'RPOPLPUSH': 0x004D,
    'LMOVE': 0x004E,
    'LMPOP': 0x004F,
    'LPOS': 0x0050,
    'BLPOP': 0x0051,
    'BRPOP': 0x0052,
    'BRPOPLPUSH': 0x0053,
    'BLMOVE': 0x0054,
    'BLMPOP': 0x0055,
    
    # Set Operations (0x0080-0x00BF)
    'SADD': 0x0080,
    'SREM': 0x0081,
    'SMEMBERS': 0x0082,
    'SISMEMBER': 0x0083,
    'SCARD': 0x0084,
    'SPOP': 0x0085,
    'SRANDMEMBER': 0x0086,
    'SINTER': 0x0087,
    'SINTERSTORE': 0x0088,
    'SUNION': 0x0089,
    'SUNIONSTORE': 0x008A,
    'SDIFF': 0x008B,
    'SDIFFSTORE': 0x008C,
    'SMOVE': 0x008D,
    'SSCAN': 0x008E,
    'SINTERCARD': 0x008F,
    'SMISMEMBER': 0x0090,
    
    # Sorted Set Operations (0x00C0-0x00FF)
    'ZADD': 0x00C0,
    'ZREM': 0x00C1,
    'ZCARD': 0x00C2,
    'ZCOUNT': 0x00C3,
    'ZINCRBY': 0x00C4,
    'ZRANGE': 0x00C5,
    'ZRANGEBYSCORE': 0x00C6,
    'ZRANGEBYLEX': 0x00C7,
    'ZREVRANGE': 0x00C8,
    'ZREVRANGEBYSCORE': 0x00C9,
    'ZREVRANGEBYLEX': 0x00CA,
    'ZRANK': 0x00CB,
    'ZREVRANK': 0x00CC,
    'ZSCORE': 0x00CD,
    'ZMSCORE': 0x00CE,
    'ZREMRANGEBYRANK': 0x00CF,
    'ZREMRANGEBYSCORE': 0x00D0,
    'ZREMRANGEBYLEX': 0x00D1,
    'ZLEXCOUNT': 0x00D2,
    'ZPOPMIN': 0x00D3,
    'ZPOPMAX': 0x00D4,
    'BZPOPMIN': 0x00D5,
    'BZPOPMAX': 0x00D6,
    'ZRANDMEMBER': 0x00D7,
    'ZDIFF': 0x00D8,
    'ZDIFFSTORE': 0x00D9,
    'ZINTER': 0x00DA,
    'ZINTERSTORE': 0x00DB,
    'ZINTERCARD': 0x00DC,
    'ZUNION': 0x00DD,
    'ZUNIONSTORE': 0x00DE,
    'ZSCAN': 0x00DF,
    'ZMPOP': 0x00E0,
    'BZMPOP': 0x00E1,
    'ZRANGESTORE': 0x00E2,
    
    # Hash Operations (0x0100-0x013F)
    'HSET': 0x0100,
    'HGET': 0x0101,
    'HMSET': 0x0102,
    'HMGET': 0x0103,
    'HGETALL': 0x0104,
    'HDEL': 0x0105,
    'HEXISTS': 0x0106,
    'HINCRBY': 0x0107,
    'HINCRBYFLOAT': 0x0108,
    'HKEYS': 0x0109,
    'HVALS': 0x010A,
    'HLEN': 0x010B,
    'HSETNX': 0x010C,
    'HSTRLEN': 0x010D,
    'HSCAN': 0x010E,
    'HRANDFIELD': 0x010F,
    
    # Generic Key Operations (0x02C0-0x02FF)
    'DEL': 0x02C0,
    'UNLINK': 0x02C1,
    'EXISTS': 0x02C2,
    'EXPIRE': 0x02C3,
    'EXPIREAT': 0x02C4,
    'EXPIRETIME': 0x02C5,
    'PEXPIRE': 0x02C6,
    'PEXPIREAT': 0x02C7,
    'PEXPIRETIME': 0x02C8,
    'TTL': 0x02C9,
    'PTTL': 0x02CA,
    'PERSIST': 0x02CB,
    'KEYS': 0x02CC,
    'SCAN': 0x02CD,
    'RANDOMKEY': 0x02CE,
    'RENAME': 0x02CF,
    'RENAMENX': 0x02D0,
    'TYPE': 0x02D1,
    'DUMP': 0x02D2,
    'RESTORE': 0x02D3,
    'MIGRATE': 0x02D4,
    'MOVE': 0x02D5,
    'COPY': 0x02D6,
    'SORT': 0x02D7,
    'SORT_RO': 0x02D8,
    'TOUCH': 0x02D9,
    'OBJECT': 0x02DA,
    'WAIT': 0x02DB,
    'WAITAOF': 0x02DC,
    
    # Connection Management (0x0300-0x033F)
    'PING': 0x0300,
    'ECHO': 0x0301,
    'AUTH': 0x0302,
    'SELECT': 0x0303,
    'QUIT': 0x0304,
    'HELLO': 0x0305,
    'RESET': 0x0306,
    'CLIENT': 0x0307,
    
    # Transaction Operations (0x0240-0x025F)
    'MULTI': 0x0240,
    'EXEC': 0x0241,
    'DISCARD': 0x0242,
    'WATCH': 0x0243,
    'UNWATCH': 0x0244,
    
    # Pub/Sub Operations (0x0200-0x023F)
    'PUBLISH': 0x0200,
    'SUBSCRIBE': 0x0201,
    'UNSUBSCRIBE': 0x0202,
    'PSUBSCRIBE': 0x0203,
    'PUNSUBSCRIBE': 0x0204,
    'PUBSUB': 0x0205,
    'SPUBLISH': 0x0206,
    'SSUBSCRIBE': 0x0207,
    'SUNSUBSCRIBE': 0x0208,
}

# Module opcode
MODULE_OPCODE = 0xF000

# Module command subcommands (4-byte: module_id << 16 | command_id)
MODULE_COMMANDS = {
    # JSON Module (Module ID: 0x0000)
    'JSON.SET': 0x00000000,
    'JSON.GET': 0x00000001,
    'JSON.MGET': 0x00000002,
    'JSON.MSET': 0x00000003,
    'JSON.DEL': 0x00000004,
    'JSON.TYPE': 0x00000006,
    'JSON.ARRAPPEND': 0x00000008,
    'JSON.ARRLEN': 0x0000000B,
    'JSON.OBJKEYS': 0x0000000E,
    'JSON.STRLEN': 0x00000010,
    
    # Bloom Filter Module (Module ID: 0x0001)
    'BF.ADD': 0x00010000,
    'BF.MADD': 0x00010001,
    'BF.EXISTS': 0x00010002,
    'BF.MEXISTS': 0x00010003,
    'BF.RESERVE': 0x00010004,
    'BF.INSERT': 0x00010005,
    'BF.CARD': 0x00010006,
    'BF.INFO': 0x00010007,
    
    # Search Module (Module ID: 0x0002)
    'FT.CREATE': 0x00020000,
    'FT.SEARCH': 0x00020001,
    'FT.DROPINDEX': 0x00020002,
    'FT.INFO': 0x00020003,
    'FT._LIST': 0x00020004,
}

def parse_resp_command(data, pos):
    """Parse a single RESP command from data starting at pos"""
    if pos >= len(data):
        return None, pos
    
    # Expect array format: *<count>\r\n
    if data[pos] != ord('*'):
        return None, pos
    
    # Find \r\n
    end = data.find(b'\r\n', pos)
    if end == -1:
        return None, pos
    
    array_len = int(data[pos+1:end])
    pos = end + 2
    
    # Parse each bulk string
    args = []
    for _ in range(array_len):
        if pos >= len(data) or data[pos] != ord('$'):
            return None, pos
        
        end = data.find(b'\r\n', pos)
        if end == -1:
            return None, pos
        
        str_len = int(data[pos+1:end])
        pos = end + 2
        
        if pos + str_len + 2 > len(data):
            return None, pos
        
        arg_data = data[pos:pos+str_len]
        args.append(arg_data)
        pos += str_len + 2  # Skip \r\n
    
    return args, pos

def serialize_respb_command(cmd_name, args, mux_id=0):
    """Serialize a command to RESPB binary format"""
    cmd_upper = cmd_name.upper()
    
    # Check if it's a module command
    if cmd_upper in MODULE_COMMANDS:
        subcommand = MODULE_COMMANDS[cmd_upper]
        # Module command: 8-byte header (opcode + mux_id + subcommand)
        buf = struct.pack('>HHI', MODULE_OPCODE, mux_id, subcommand)
        return serialize_module_payload(cmd_upper, args, buf)
    
    opcode = RESPB_OPCODES.get(cmd_upper, 0xFFFF)
    
    # Header: opcode (2B) + mux_id (2B)
    buf = struct.pack('>HH', opcode, mux_id)
    
    # Serialize based on command type
    if cmd_upper in ['GET', 'INCR', 'DECR', 'LLEN', 'SCARD', 'TTL']:
        # Single key: [2B keylen][key]
        if len(args) > 0:
            key = args[0]
            buf += struct.pack('>H', len(key))
            buf += key
    
    elif cmd_upper == 'SET':
        # [2B keylen][key][4B vallen][value][1B flags][8B expiry]
        if len(args) >= 2:
            key, value = args[0], args[1]
            buf += struct.pack('>H', len(key))
            buf += key
            buf += struct.pack('>I', len(value))
            buf += value
            buf += struct.pack('>BQ', 0, 0)  # No flags, no expiry
    
    elif cmd_upper in ['APPEND', 'SETNX']:
        # [2B keylen][key][4B vallen][value]
        if len(args) >= 2:
            key, value = args[0], args[1]
            buf += struct.pack('>H', len(key))
            buf += key
            buf += struct.pack('>I', len(value))
            buf += value
    
    elif cmd_upper in ['INCRBY', 'DECRBY', 'EXPIRE']:
        # [2B keylen][key][8B integer]
        if len(args) >= 1:
            key = args[0]
            buf += struct.pack('>H', len(key))
            buf += key
            buf += struct.pack('>q', 1)  # Default increment/ttl
    
    elif cmd_upper in ['MGET', 'DEL', 'EXISTS']:
        # [2B count][ [2B keylen][key] ... ]
        buf += struct.pack('>H', len(args))
        for arg in args:
            buf += struct.pack('>H', len(arg))
            buf += arg
    
    elif cmd_upper == 'MSET':
        # [2B npairs][ [2B keylen][key][4B vallen][value] ... ]
        npairs = len(args) // 2
        buf += struct.pack('>H', npairs)
        for i in range(0, len(args), 2):
            if i + 1 < len(args):
                key, value = args[i], args[i+1]
                buf += struct.pack('>H', len(key))
                buf += key
                buf += struct.pack('>I', len(value))
                buf += value
    
    elif cmd_upper in ['LPUSH', 'RPUSH']:
        # [2B keylen][key][2B count][ [2B elemlen][element] ... ]
        if len(args) >= 1:
            key = args[0]
            buf += struct.pack('>H', len(key))
            buf += key
            count = len(args) - 1
            buf += struct.pack('>H', count)
            for elem in args[1:]:
                buf += struct.pack('>H', len(elem))
                buf += elem
    
    elif cmd_upper == 'SADD':
        # [2B keylen][key][2B count][ [2B memberlen][member] ... ]
        if len(args) >= 1:
            key = args[0]
            buf += struct.pack('>H', len(key))
            buf += key
            count = len(args) - 1
            buf += struct.pack('>H', count)
            for member in args[1:]:
                buf += struct.pack('>H', len(member))
                buf += member
    
    elif cmd_upper == 'HSET':
        # [2B keylen][key][2B npairs][ [2B fieldlen][field][4B vallen][value] ... ]
        if len(args) >= 1:
            key = args[0]
            buf += struct.pack('>H', len(key))
            buf += key
            npairs = (len(args) - 1) // 2
            buf += struct.pack('>H', npairs)
            for i in range(1, len(args), 2):
                if i + 1 < len(args):
                    field, value = args[i], args[i+1]
                    buf += struct.pack('>H', len(field))
                    buf += field
                    buf += struct.pack('>I', len(value))
                    buf += value
    
    elif cmd_upper == 'HGET':
        # [2B keylen][key][2B fieldlen][field]
        if len(args) >= 2:
            key, field = args[0], args[1]
            buf += struct.pack('>H', len(key))
            buf += key
            buf += struct.pack('>H', len(field))
            buf += field
    
    elif cmd_upper == 'PING':
        # No payload
        pass
    
    else:
        # RESP passthrough format for unknown commands (0xFFFF)
        # Generate RESP text format first
        resp_text = b'*' + str(len(args)).encode() + b'\r\n'
        for arg in args:
            resp_text += b'$' + str(len(arg)).encode() + b'\r\n'
            resp_text += arg + b'\r\n'
        
        # RESP passthrough: [0xFFFF][mux_id][resp_length][RESP text]
        buf = struct.pack('>HHI', 0xFFFF, mux_id, len(resp_text))
        buf += resp_text
    
    return buf

def serialize_module_payload(cmd_name, args, header_buf):
    """Serialize module command payload based on command type"""
    buf = header_buf
    cmd_upper = cmd_name.upper()
    
    # JSON commands
    if cmd_upper == 'JSON.SET':
        # [2B keylen][key][2B pathlen][path][4B jsonlen][json][1B flags]
        if len(args) >= 3:
            key, path, json_data = args[0], args[1], args[2]
            buf += struct.pack('>H', len(key))
            buf += key
            buf += struct.pack('>H', len(path))
            buf += path
            buf += struct.pack('>I', len(json_data))
            buf += json_data
            buf += struct.pack('>B', 0)  # Flags
    elif cmd_upper == 'JSON.GET':
        # [2B keylen][key][2B numpaths]([2B pathlen][path])...
        if len(args) >= 1:
            key = args[0]
            buf += struct.pack('>H', len(key))
            buf += key
            num_paths = len(args) - 1
            buf += struct.pack('>H', num_paths)
            for path in args[1:]:
                buf += struct.pack('>H', len(path))
                buf += path
    elif cmd_upper == 'JSON.DEL':
        # [2B keylen][key][2B pathlen?][path?]
        if len(args) >= 1:
            key = args[0]
            buf += struct.pack('>H', len(key))
            buf += key
            if len(args) >= 2:
                path = args[1]
                buf += struct.pack('>H', len(path))
                buf += path
    
    # Bloom Filter commands
    elif cmd_upper == 'BF.ADD':
        # [2B keylen][key][2B itemlen][item]
        if len(args) >= 2:
            key, item = args[0], args[1]
            buf += struct.pack('>H', len(key))
            buf += key
            buf += struct.pack('>H', len(item))
            buf += item
    elif cmd_upper == 'BF.EXISTS':
        # [2B keylen][key][2B itemlen][item]
        if len(args) >= 2:
            key, item = args[0], args[1]
            buf += struct.pack('>H', len(key))
            buf += key
            buf += struct.pack('>H', len(item))
            buf += item
    elif cmd_upper == 'BF.MADD':
        # [2B keylen][key][2B count]([2B itemlen][item])...
        if len(args) >= 2:
            key = args[0]
            buf += struct.pack('>H', len(key))
            buf += key
            count = len(args) - 1
            buf += struct.pack('>H', count)
            for item in args[1:]:
                buf += struct.pack('>H', len(item))
                buf += item
    
    # Search/FT commands
    elif cmd_upper == 'FT.SEARCH':
        # [2B idxlen][index][2B querylen][query][...options]
        if len(args) >= 2:
            index, query = args[0], args[1]
            buf += struct.pack('>H', len(index))
            buf += index
            buf += struct.pack('>H', len(query))
            buf += query
    elif cmd_upper == 'FT.INFO':
        # [2B idxlen][index]
        if len(args) >= 1:
            index = args[0]
            buf += struct.pack('>H', len(index))
            buf += index
    
    # Generic fallback for other module commands
    else:
        for arg in args:
            buf += struct.pack('>H', len(arg))
            buf += arg
    
    return buf

def convert_resp_to_respb(resp_data):
    """Convert RESP workload to RESPB workload"""
    respb_data = bytearray()
    pos = 0
    
    while pos < len(resp_data):
        args, new_pos = parse_resp_command(resp_data, pos)
        if args is None or new_pos == pos:
            break
        
        if len(args) > 0:
            cmd_name = args[0].decode('utf-8', errors='ignore')
            respb_cmd = serialize_respb_command(cmd_name, args[1:])
            respb_data.extend(respb_cmd)
        
        pos = new_pos
    
    return bytes(respb_data)

def generate_resp_workload(output_file, workload_type, size_mb=10):
    """Generate RESP workload"""
    print(f"Generating {workload_type} RESP workload ({size_mb}MB)...")
    
    target_size = size_mb * 1024 * 1024
    data = bytearray()
    
    if workload_type == 'small':
        # Small GET commands
        cmd_count = 0
        while len(data) < target_size:
            cmd = f"*2\r\n$3\r\nGET\r\n$6\r\nkey_{cmd_count % 100:02d}\r\n".encode()
            data.extend(cmd)
            cmd_count += 1
    
    elif workload_type == 'medium':
        # Medium SET commands
        value = 'X' * 50
        cmd_count = 0
        while len(data) < target_size:
            cmd = f"*3\r\n$3\r\nSET\r\n$8\r\nkey_{cmd_count % 1000:04d}\r\n$50\r\n{value}\r\n".encode()
            data.extend(cmd)
            cmd_count += 1
    
    elif workload_type == 'large':
        # Large SET commands
        value = 'X' * 1024
        cmd_count = 0
        while len(data) < target_size:
            key = f"largekey{cmd_count % 100}"
            cmd = f"*3\r\n$3\r\nSET\r\n${len(key)}\r\n{key}\r\n$1024\r\n{value}\r\n".encode()
            data.extend(cmd)
            cmd_count += 1
    
    elif workload_type == 'mixed':
        # Mix of commands including module commands
        cmd_count = 0
        while len(data) < target_size:
            choice = cmd_count % 8
            if choice == 0:
                cmd = f"*2\r\n$3\r\nGET\r\n$6\r\nkey_{cmd_count % 100:02d}\r\n".encode()
            elif choice == 1:
                cmd = f"*3\r\n$3\r\nSET\r\n$6\r\nkey_{cmd_count % 100:02d}\r\n$6\r\nval_{cmd_count % 100:02d}\r\n".encode()
            elif choice == 2:
                cmd = f"*2\r\n$3\r\nDEL\r\n$6\r\nkey_{cmd_count % 100:02d}\r\n".encode()
            elif choice == 3:
                cmd = b"*4\r\n$4\r\nMGET\r\n$5\r\nkey_0\r\n$5\r\nkey_1\r\n$5\r\nkey_2\r\n"
            elif choice == 4:
                # JSON.SET
                key = f"json_{cmd_count % 50:02d}".encode()
                path = b".name"
                json_val = b'"John Doe"'
                cmd = f"*4\r\n$8\r\nJSON.SET\r\n${len(key)}\r\n".encode() + key + \
                      f"\r\n${len(path)}\r\n".encode() + path + \
                      f"\r\n${len(json_val)}\r\n".encode() + json_val + b"\r\n"
            elif choice == 5:
                # JSON.GET
                key = f"json_{cmd_count % 50:02d}".encode()
                path = b".name"
                cmd = f"*3\r\n$8\r\nJSON.GET\r\n${len(key)}\r\n".encode() + key + \
                      f"\r\n${len(path)}\r\n".encode() + path + b"\r\n"
            elif choice == 6:
                # BF.ADD
                key = f"bf_{cmd_count % 50:02d}".encode()
                item = f"item_{cmd_count % 100:03d}".encode()
                cmd = f"*3\r\n$6\r\nBF.ADD\r\n${len(key)}\r\n".encode() + key + \
                      f"\r\n${len(item)}\r\n".encode() + item + b"\r\n"
            else:  # choice == 7
                # FT.SEARCH
                index = b"idx1"
                query = b"hello"
                cmd = f"*3\r\n$9\r\nFT.SEARCH\r\n${len(index)}\r\n".encode() + index + \
                      f"\r\n${len(query)}\r\n".encode() + query + b"\r\n"
            data.extend(cmd)
            cmd_count += 1
    
    with open(output_file, 'wb') as f:
        f.write(data)
    
    print(f"Generated RESP workload: {output_file} ({len(data)} bytes)")
    return bytes(data)

def main():
    parser = argparse.ArgumentParser(description='Generate protocol benchmark workloads')
    parser.add_argument('--output', '-o', default='data', help='Output directory')
    parser.add_argument('--size', '-s', type=int, default=10, help='Size in MB (default: 10)')
    parser.add_argument('--types', '-t', nargs='+', 
                       default=['small', 'medium', 'large', 'mixed'],
                       help='Workload types to generate')
    
    args = parser.parse_args()
    
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    for wl_type in args.types:
        # Generate RESP workload
        resp_file = output_dir / f'workload_{wl_type}_resp.bin'
        resp_data = generate_resp_workload(resp_file, wl_type, args.size)
        
        # Convert to RESPB
        respb_file = output_dir / f'workload_{wl_type}_respb.bin'
        print(f"Converting to RESPB...")
        respb_data = convert_resp_to_respb(resp_data)
        
        with open(respb_file, 'wb') as f:
            f.write(respb_data)
        
        print(f"Generated RESPB workload: {respb_file} ({len(respb_data)} bytes)")
        
        size_reduction = (1 - len(respb_data) / len(resp_data)) * 100
        print(f"Size reduction: {size_reduction:.1f}%\n")
    
    print("Workload generation complete!")

if __name__ == '__main__':
    main()
