#!/usr/bin/env python3
"""
RESPB Converter - Convert RESP (text) protocol to RESPB (binary) protocol

This module provides tools to parse RESP protocol commands and convert them
to the binary RESPB format as defined in respb-specs.md and respb-commands.md.
"""

import struct
import csv
import argparse
import sys
from typing import List, Dict, Tuple, Optional, Union
from dataclasses import dataclass
from enum import Enum


# RESPB opcode constants
MODULE_OPCODE = 0xF000
RESP_PASSTHROUGH_OPCODE = 0xFFFF

# Module ID constants
JSON_MODULE_ID = 0x0000
BF_MODULE_ID = 0x0001
FT_MODULE_ID = 0x0002

# Module command mappings (4-byte subcommand: module_id << 16 | command_id)
MODULE_COMMANDS: Dict[str, int] = {
    # JSON Module (Module ID: 0x0000)
    'JSON.SET': 0x00000000,
    'JSON.GET': 0x00000001,
    'JSON.MGET': 0x00000002,
    'JSON.MSET': 0x00000003,
    'JSON.DEL': 0x00000004,
    'JSON.FORGET': 0x00000005,
    'JSON.TYPE': 0x00000006,
    'JSON.CLEAR': 0x00000007,
    'JSON.ARRAPPEND': 0x00000008,
    'JSON.ARRINDEX': 0x00000009,
    'JSON.ARRINSERT': 0x0000000A,
    'JSON.ARRLEN': 0x0000000B,
    'JSON.ARRPOP': 0x0000000C,
    'JSON.ARRTRIM': 0x0000000D,
    'JSON.OBJKEYS': 0x0000000E,
    'JSON.OBJLEN': 0x0000000F,
    'JSON.STRLEN': 0x00000010,
    'JSON.STRAPPEND': 0x00000011,
    'JSON.NUMINCRBY': 0x00000012,
    'JSON.NUMMULTBY': 0x00000013,
    'JSON.TOGGLE': 0x00000014,
    'JSON.DEBUG': 0x00000015,
    'JSON.RESP': 0x00000016,
    
    # Bloom Filter Module (Module ID: 0x0001)
    'BF.ADD': 0x00010000,
    'BF.MADD': 0x00010001,
    'BF.EXISTS': 0x00010002,
    'BF.MEXISTS': 0x00010003,
    'BF.RESERVE': 0x00010004,
    'BF.INSERT': 0x00010005,
    'BF.CARD': 0x00010006,
    'BF.INFO': 0x00010007,
    'BF.LOAD': 0x00010008,
    
    # Search Module (Module ID: 0x0002)
    'FT.CREATE': 0x00020000,
    'FT.SEARCH': 0x00020001,
    'FT.DROPINDEX': 0x00020002,
    'FT.INFO': 0x00020003,
    'FT._LIST': 0x00020004,
}

# Load command opcode mapping from CSV
COMMAND_OPCODES: Dict[str, int] = {}

def load_command_opcodes(csv_file: str = "valkey_commands.csv"):
    """Load command to opcode mapping."""
    global COMMAND_OPCODES
    
    # Static opcode mapping based on respb-commands.md
    COMMAND_OPCODES = {
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
        
        # Bitmap Operations (0x0140-0x015F)
        'SETBIT': 0x0140,
        'GETBIT': 0x0141,
        'BITCOUNT': 0x0142,
        'BITPOS': 0x0143,
        'BITOP': 0x0144,
        'BITFIELD': 0x0145,
        'BITFIELD_RO': 0x0146,
        
        # HyperLogLog (0x0160-0x017F)
        'PFADD': 0x0160,
        'PFCOUNT': 0x0161,
        'PFMERGE': 0x0162,
        
        # Generic Operations (0x02C0-0x02FF)
        'DEL': 0x02C0,
        'UNLINK': 0x02C1,
        'EXISTS': 0x02C2,
        'EXPIRE': 0x02C3,
        'EXPIREAT': 0x02C4,
        'TTL': 0x02C9,
        'PTTL': 0x02CA,
        'PERSIST': 0x02CB,
        'KEYS': 0x02CC,
        'SCAN': 0x02CD,
        'RANDOMKEY': 0x02CE,
        'RENAME': 0x02CF,
        'RENAMENX': 0x02D0,
        'TYPE': 0x02D1,
        
        # Connection (0x0300-0x033F)
        'PING': 0x0300,
        'ECHO': 0x0301,
        'AUTH': 0x0302,
        'SELECT': 0x0303,
        'QUIT': 0x0304,
        
        # Pub/Sub (0x0200-0x023F)
        'PUBLISH': 0x0200,
        'SUBSCRIBE': 0x0201,
        'UNSUBSCRIBE': 0x0202,
        
        # Transaction (0x0240-0x025F)
        'MULTI': 0x0240,
        'EXEC': 0x0241,
        'DISCARD': 0x0242,
        'WATCH': 0x0243,
        'UNWATCH': 0x0244,
        
        # Server (0x03C0-0x04FF)
        'DBSIZE': 0x03C0,
        'FLUSHDB': 0x03C1,
        'FLUSHALL': 0x03C2,
        'SAVE': 0x03C3,
        'BGSAVE': 0x03C4,
        'INFO': 0x03C8,
        'CONFIG': 0x03C9,
        'TIME': 0x03CB,
    }


@dataclass
class RESPCommand:
    """Represents a parsed RESP command."""
    command: str
    args: List[bytes]
    
    def to_resp_text(self) -> bytes:
        """Convert back to RESP text format."""
        parts = [self.command.encode()] + self.args
        result = f"*{len(parts)}\r\n".encode()
        for part in parts:
            result += f"${len(part)}\r\n".encode()
            result += part + b"\r\n"
        return result


class RESPParser:
    """Parse RESP protocol text format."""
    
    @staticmethod
    def parse_bulk_string(data: bytes, pos: int) -> Tuple[Optional[bytes], int]:
        """Parse a RESP bulk string starting at position pos."""
        if data[pos:pos+1] != b'$':
            raise ValueError(f"Expected '$' at position {pos}")
        
        pos += 1
        # Find the length
        end = data.find(b'\r\n', pos)
        if end == -1:
            raise ValueError("Incomplete bulk string length")
        
        length = int(data[pos:end])
        pos = end + 2
        
        if length == -1:
            return None, pos
        
        if pos + length > len(data):
            raise ValueError("Incomplete bulk string data")
        
        value = data[pos:pos+length]
        pos += length + 2  # Skip data and \r\n
        return value, pos
    
    @staticmethod
    def parse_array(data: bytes, pos: int = 0) -> Tuple[List[bytes], int]:
        """Parse a RESP array starting at position pos."""
        if data[pos:pos+1] != b'*':
            raise ValueError(f"Expected '*' at position {pos}")
        
        pos += 1
        end = data.find(b'\r\n', pos)
        if end == -1:
            raise ValueError("Incomplete array count")
        
        count = int(data[pos:end])
        pos = end + 2
        
        elements = []
        for _ in range(count):
            element, pos = RESPParser.parse_bulk_string(data, pos)
            if element is not None:
                elements.append(element)
        
        return elements, pos
    
    @staticmethod
    def parse_command(data: bytes) -> RESPCommand:
        """Parse a complete RESP command."""
        elements, _ = RESPParser.parse_array(data)
        if not elements:
            raise ValueError("Empty command")
        
        command = elements[0].decode('utf-8').upper()
        args = elements[1:]
        return RESPCommand(command=command, args=args)


class RESPBSerializer:
    """Serialize commands to RESPB binary format."""
    
    def __init__(self, mux_id: int = 0):
        self.mux_id = mux_id
        load_command_opcodes()
    
    def encode_header(self, opcode: int) -> bytes:
        """Encode RESPB frame header (opcode + mux_id)."""
        return struct.pack('!HH', opcode, self.mux_id)
    
    def encode_string_2b(self, data: bytes) -> bytes:
        """Encode a string with 2-byte length prefix."""
        return struct.pack('!H', len(data)) + data
    
    def encode_string_4b(self, data: bytes) -> bytes:
        """Encode a string with 4-byte length prefix."""
        return struct.pack('!I', len(data)) + data
    
    def encode_int64(self, value: int) -> bytes:
        """Encode a 64-bit signed integer."""
        return struct.pack('!q', value)
    
    def encode_float64(self, value: float) -> bytes:
        """Encode a 64-bit float."""
        return struct.pack('!d', value)
    
    def serialize_generic(self, command: RESPCommand) -> bytes:
        """Generic serialization for simple key-based commands."""
        cmd = command.command.upper()
        
        # Check for module commands first
        if cmd in MODULE_COMMANDS:
            return self.serialize_module_command(command)
        
        # Check for core commands
        opcode = COMMAND_OPCODES.get(cmd)
        if opcode is None:
            # Unknown command - use RESP passthrough
            return self.serialize_resp_passthrough(command)
        
        frame = self.encode_header(opcode)
        
        # Try to handle common patterns
        # Single key commands: GET, STRLEN, TYPE, TTL, etc.
        if cmd in ['GET', 'STRLEN', 'TYPE', 'TTL', 'PTTL', 'PERSIST', 
                   'INCR', 'DECR', 'GETDEL', 'DUMP', 'LLEN', 'SCARD',
                   'ZCARD', 'HGETALL', 'HKEYS', 'HVALS', 'HLEN', 'RANDOMKEY',
                   'SMEMBERS', 'DBSIZE', 'QUIT', 'MULTI', 'EXEC', 'DISCARD',
                   'UNWATCH', 'PING', 'TIME', 'SAVE', 'BGSAVE']:
            if len(command.args) >= 1:
                frame += self.encode_string_2b(command.args[0])
            # PING, TIME, etc. might have no args
            return frame
        
        # Key + value commands: SET, GETSET, APPEND, etc.
        elif cmd in ['APPEND', 'GETSET', 'SETNX']:
            if len(command.args) >= 2:
                frame += self.encode_string_2b(command.args[0])  # key
                frame += self.encode_string_4b(command.args[1])  # value
            return frame
        
        # SET command (with options)
        elif cmd == 'SET':
            return self.serialize_set(command)
        
        # Key + integer commands
        elif cmd in ['INCRBY', 'DECRBY', 'EXPIRE', 'EXPIREAT']:
            if len(command.args) >= 2:
                frame += self.encode_string_2b(command.args[0])  # key
                frame += self.encode_int64(int(command.args[1]))  # integer
            return frame
        
        # Multi-key commands: MGET, DEL, EXISTS, etc.
        elif cmd in ['MGET', 'DEL', 'UNLINK', 'EXISTS', 'KEYS']:
            frame += struct.pack('!H', len(command.args))  # count
            for arg in command.args:
                frame += self.encode_string_2b(arg)
            return frame
        
        # MSET command
        elif cmd in ['MSET', 'MSETNX']:
            num_pairs = len(command.args) // 2
            frame += struct.pack('!H', num_pairs)
            for i in range(0, len(command.args), 2):
                if i + 1 < len(command.args):
                    frame += self.encode_string_2b(command.args[i])      # key
                    frame += self.encode_string_4b(command.args[i + 1])  # value
            return frame
        
        # List operations
        elif cmd in ['LPUSH', 'RPUSH', 'LPUSHX', 'RPUSHX']:
            if len(command.args) >= 2:
                frame += self.encode_string_2b(command.args[0])  # key
                frame += struct.pack('!H', len(command.args) - 1)  # count
                for arg in command.args[1:]:
                    frame += self.encode_string_2b(arg)
            return frame
        
        elif cmd in ['LPOP', 'RPOP']:
            if len(command.args) >= 1:
                frame += self.encode_string_2b(command.args[0])  # key
                if len(command.args) >= 2:
                    frame += struct.pack('!H', int(command.args[1]))  # count
            return frame
        
        elif cmd == 'LRANGE':
            if len(command.args) >= 3:
                frame += self.encode_string_2b(command.args[0])  # key
                frame += self.encode_int64(int(command.args[1]))  # start
                frame += self.encode_int64(int(command.args[2]))  # stop
            return frame
        
        # Set operations
        elif cmd == 'SADD':
            if len(command.args) >= 2:
                frame += self.encode_string_2b(command.args[0])  # key
                frame += struct.pack('!H', len(command.args) - 1)  # count
                for arg in command.args[1:]:
                    frame += self.encode_string_2b(arg)
            return frame
        
        elif cmd == 'SISMEMBER':
            if len(command.args) >= 2:
                frame += self.encode_string_2b(command.args[0])  # key
                frame += self.encode_string_2b(command.args[1])  # member
            return frame
        
        # Sorted set operations
        elif cmd == 'ZADD':
            if len(command.args) >= 3:
                frame += self.encode_string_2b(command.args[0])  # key
                frame += b'\x00'  # flags
                num_pairs = (len(command.args) - 1) // 2
                frame += struct.pack('!H', num_pairs)
                for i in range(1, len(command.args), 2):
                    if i + 1 < len(command.args):
                        score = float(command.args[i])
                        frame += self.encode_float64(score)
                        frame += self.encode_string_2b(command.args[i + 1])
            return frame
        
        elif cmd in ['ZSCORE', 'ZRANK', 'ZREVRANK']:
            if len(command.args) >= 2:
                frame += self.encode_string_2b(command.args[0])  # key
                frame += self.encode_string_2b(command.args[1])  # member
                if cmd in ['ZRANK', 'ZREVRANK']:
                    frame += b'\x00'  # withscore flag
            return frame
        
        elif cmd in ['ZRANGE', 'ZREVRANGE']:
            if len(command.args) >= 3:
                frame += self.encode_string_2b(command.args[0])  # key
                frame += self.encode_int64(int(command.args[1]))  # start
                frame += self.encode_int64(int(command.args[2]))  # stop
                # Check for WITHSCORES
                flags = 0x00
                if len(command.args) > 3 and command.args[3].upper() == b'WITHSCORES':
                    flags = 0x01
                frame += bytes([flags])
            return frame
        
        # Hash operations
        elif cmd == 'HSET':
            if len(command.args) >= 3:
                frame += self.encode_string_2b(command.args[0])  # key
                num_pairs = (len(command.args) - 1) // 2
                frame += struct.pack('!H', num_pairs)
                for i in range(1, len(command.args), 2):
                    if i + 1 < len(command.args):
                        frame += self.encode_string_2b(command.args[i])      # field
                        frame += self.encode_string_4b(command.args[i + 1])  # value
            return frame
        
        elif cmd == 'HGET':
            if len(command.args) >= 2:
                frame += self.encode_string_2b(command.args[0])  # key
                frame += self.encode_string_2b(command.args[1])  # field
            return frame
        
        elif cmd == 'HDEL':
            if len(command.args) >= 2:
                frame += self.encode_string_2b(command.args[0])  # key
                frame += struct.pack('!H', len(command.args) - 1)  # count
                for arg in command.args[1:]:
                    frame += self.encode_string_2b(arg)
            return frame
        
        # Pub/Sub
        elif cmd == 'PUBLISH':
            if len(command.args) >= 2:
                frame += self.encode_string_2b(command.args[0])  # channel
                frame += self.encode_string_4b(command.args[1])  # message
            return frame
        
        elif cmd in ['SUBSCRIBE', 'UNSUBSCRIBE']:
            frame += struct.pack('!H', len(command.args))  # count
            for arg in command.args:
                frame += self.encode_string_2b(arg)
            return frame
        
        # Connection
        elif cmd == 'ECHO':
            if len(command.args) >= 1:
                frame += self.encode_string_2b(command.args[0])
            return frame
        
        elif cmd == 'SELECT':
            if len(command.args) >= 1:
                frame += struct.pack('!H', int(command.args[0]))
            return frame
        
        # Fallback to RESP passthrough
        return self.serialize_resp_passthrough(command)
    
    def serialize_module_command(self, command: RESPCommand) -> bytes:
        """Serialize module command with 8-byte header (opcode + mux_id + 4-byte subcommand)."""
        subcommand = MODULE_COMMANDS.get(command.command)
        if subcommand is None:
            raise ValueError(f"Unknown module command: {command.command}")
        
        # 8-byte header: opcode (2B) + mux_id (2B) + subcommand (4B)
        frame = struct.pack('!HHI', MODULE_OPCODE, self.mux_id, subcommand)
        
        cmd = command.command.upper()
        
        # JSON.SET: [2B keylen][key][2B pathlen][path][4B jsonlen][json][1B flags]
        if cmd == 'JSON.SET':
            if len(command.args) >= 3:
                frame += self.encode_string_2b(command.args[0])  # key
                frame += self.encode_string_2b(command.args[1])  # path
                frame += self.encode_string_4b(command.args[2])  # json
                flags = 0x00
                if len(command.args) > 3:
                    # Handle optional flags (NX, XX, etc.)
                    for opt in command.args[3:]:
                        opt_upper = opt.upper()
                        if opt_upper == b'NX':
                            flags |= 0x01
                        elif opt_upper == b'XX':
                            flags |= 0x02
                frame += bytes([flags])
            return frame
        
        # JSON.GET: [2B keylen][key][2B numpaths]([2B pathlen][path])...
        elif cmd == 'JSON.GET':
            if len(command.args) >= 1:
                frame += self.encode_string_2b(command.args[0])  # key
                num_paths = len(command.args) - 1
                frame += struct.pack('!H', num_paths)
                for path in command.args[1:]:
                    frame += self.encode_string_2b(path)
            return frame
        
        # JSON.DEL: [2B keylen][key][2B pathlen?][path?]
        elif cmd == 'JSON.DEL':
            if len(command.args) >= 1:
                frame += self.encode_string_2b(command.args[0])  # key
                if len(command.args) >= 2:
                    frame += self.encode_string_2b(command.args[1])  # path
            return frame
        
        # BF.ADD: [2B keylen][key][2B itemlen][item]
        elif cmd == 'BF.ADD':
            if len(command.args) >= 2:
                frame += self.encode_string_2b(command.args[0])  # key
                frame += self.encode_string_2b(command.args[1])  # item
            return frame
        
        # BF.EXISTS: [2B keylen][key][2B itemlen][item]
        elif cmd == 'BF.EXISTS':
            if len(command.args) >= 2:
                frame += self.encode_string_2b(command.args[0])  # key
                frame += self.encode_string_2b(command.args[1])  # item
            return frame
        
        # FT.SEARCH: [2B idxlen][index][2B querylen][query][...options]
        elif cmd == 'FT.SEARCH':
            if len(command.args) >= 2:
                frame += self.encode_string_2b(command.args[0])  # index
                frame += self.encode_string_2b(command.args[1])  # query
                # Additional options would be encoded here
            return frame
        
        # FT.INFO: [2B idxlen][index]
        elif cmd == 'FT.INFO':
            if len(command.args) >= 1:
                frame += self.encode_string_2b(command.args[0])  # index
            return frame
        
        # Generic fallback: encode all args as 2-byte length prefixed strings
        else:
            for arg in command.args:
                frame += self.encode_string_2b(arg)
            return frame
    
    def serialize_set(self, command: RESPCommand) -> bytes:
        """Serialize SET command with optional parameters."""
        opcode = COMMAND_OPCODES['SET']
        frame = self.encode_header(opcode)
        
        if len(command.args) < 2:
            return frame
        
        frame += self.encode_string_2b(command.args[0])  # key
        frame += self.encode_string_4b(command.args[1])  # value
        
        # Parse optional flags
        flags = 0x00
        expiry = 0
        
        i = 2
        while i < len(command.args):
            opt = command.args[i].upper()
            if opt == b'NX':
                flags |= 0x01
            elif opt == b'XX':
                flags |= 0x02
            elif opt == b'EX' and i + 1 < len(command.args):
                flags |= 0x04
                expiry = int(command.args[i + 1])
                i += 1
            elif opt == b'PX' and i + 1 < len(command.args):
                flags |= 0x08
                expiry = int(command.args[i + 1])
                i += 1
            i += 1
        
        frame += bytes([flags])
        frame += self.encode_int64(expiry)
        
        return frame
    
    def serialize_resp_passthrough(self, command: RESPCommand) -> bytes:
        """Serialize using RESP passthrough opcode (0xFFFF) for unknown commands.
        
        Format: [0xFFFF][mux_id][4B resp_length][RESP text data...]
        """
        # Generate RESP text format
        resp_data = command.to_resp_text()
        
        # 8-byte header: opcode (2B) + mux_id (2B) + resp_length (4B)
        frame = struct.pack('!HHI', RESP_PASSTHROUGH_OPCODE, self.mux_id, len(resp_data))
        
        # Append raw RESP text data
        frame += resp_data
        
        return frame
    
    def serialize_extension(self, command: RESPCommand) -> bytes:
        """Legacy method name - redirects to RESP passthrough.
        
        Note: This method is kept for backward compatibility.
        Use serialize_resp_passthrough() for new code.
        """
        return self.serialize_resp_passthrough(command)
    
    def serialize(self, command: RESPCommand) -> bytes:
        """Serialize a command to RESPB format."""
        return self.serialize_generic(command)


class ProtocolComparator:
    """Compare RESP and RESPB protocol efficiency."""
    
    @staticmethod
    def compare_command(resp_data: bytes, respb_data: bytes, command: str) -> Dict:
        """Compare sizes and calculate savings."""
        resp_size = len(resp_data)
        respb_size = len(respb_data)
        savings = resp_size - respb_size
        savings_pct = (savings / resp_size * 100) if resp_size > 0 else 0
        
        return {
            'command': command,
            'resp_size': resp_size,
            'respb_size': respb_size,
            'savings_bytes': savings,
            'savings_percent': savings_pct,
            'resp_hex': resp_data.hex()[:100],
            'respb_hex': respb_data.hex()[:100]
        }
    
    @staticmethod
    def format_comparison(result: Dict) -> str:
        """Format comparison result as a readable string."""
        return (
            f"Command: {result['command']}\n"
            f"  RESP:  {result['resp_size']:4d} bytes\n"
            f"  RESPB: {result['respb_size']:4d} bytes\n"
            f"  Savings: {result['savings_bytes']:4d} bytes ({result['savings_percent']:5.1f}%)\n"
        )


def run_demo():
    """Run demo with hardcoded test commands."""
    print("=" * 70)
    print("RESPB Converter - RESP to RESPB Protocol Comparison (DEMO)")
    print("=" * 70)
    print()
    
    # Test commands
    test_commands = [
        b"*2\r\n$3\r\nGET\r\n$6\r\nmykey\r\n",
        b"*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$5\r\nhello\r\n",
        b"*5\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n$2\r\nEX\r\n$2\r\n60\r\n",
        b"*4\r\n$4\r\nMGET\r\n$2\r\nk1\r\n$2\r\nk2\r\n$2\r\nk3\r\n",
        b"*7\r\n$4\r\nMSET\r\n$2\r\nk1\r\n$1\r\n1\r\n$2\r\nk2\r\n$1\r\n2\r\n$2\r\nk3\r\n$1\r\n3\r\n",
        b"*3\r\n$5\r\nLPUSH\r\n$6\r\nmylist\r\n$3\r\nfoo\r\n",
        b"*4\r\n$4\r\nSADD\r\n$5\r\nmyset\r\n$1\r\nx\r\n$1\r\ny\r\n",
        b"*5\r\n$4\r\nZADD\r\n$5\r\nmyzset\r\n$3\r\n1.0\r\n$3\r\none\r\n",
        b"*4\r\n$4\r\nHSET\r\n$5\r\nmyhash\r\n$5\r\nfield\r\n$5\r\nvalue\r\n",
        b"*3\r\n$7\r\nPUBLISH\r\n$4\r\nnews\r\n$5\r\nhello\r\n",
        b"*3\r\n$3\r\nDEL\r\n$4\r\nkey1\r\n$4\r\nkey2\r\n",
        b"*1\r\n$4\r\nPING\r\n",
        b"*2\r\n$4\r\nECHO\r\n$5\r\nhello\r\n",
        b"*4\r\n$6\r\nLRANGE\r\n$6\r\nmylist\r\n$1\r\n0\r\n$2\r\n-1\r\n",
    ]
    
    parser = RESPParser()
    serializer = RESPBSerializer()
    comparator = ProtocolComparator()
    
    results = []
    total_resp = 0
    total_respb = 0
    
    for resp_data in test_commands:
        try:
            # Parse RESP command
            command = parser.parse_command(resp_data)
            
            # Serialize to RESPB
            respb_data = serializer.serialize(command)
            
            # Compare
            result = comparator.compare_command(resp_data, respb_data, 
                                               f"{command.command} {' '.join(a.decode('utf-8', errors='replace')[:20] for a in command.args[:3])}")
            results.append(result)
            
            total_resp += result['resp_size']
            total_respb += result['respb_size']
            
            print(comparator.format_comparison(result))
            
        except Exception as e:
            print(f"Error processing command: {e}\n")
    
    print("=" * 70)
    print("SUMMARY")
    print("=" * 70)
    print(f"Total RESP size:  {total_resp:6d} bytes")
    print(f"Total RESPB size: {total_respb:6d} bytes")
    print(f"Total savings:    {total_resp - total_respb:6d} bytes ({(total_resp - total_respb) / total_resp * 100:.1f}%)")
    print()
    print(f"Average RESP size:  {total_resp / len(results):.1f} bytes")
    print(f"Average RESPB size: {total_respb / len(results):.1f} bytes")
    print(f"Average savings:    {(total_resp - total_respb) / len(results):.1f} bytes per command")


def convert_aof_file(input_file, output_file):
    """Convert AOF file from RESP to RESPB format."""
    import os
    import time
    
    print("=" * 70)
    print("RESPB Converter - AOF File Conversion")
    print("=" * 70)
    print(f"Input:  {input_file}")
    print(f"Output: {output_file}")
    
    # Get file size for progress tracking
    try:
        file_size = os.path.getsize(input_file)
        print(f"Size:   {file_size:,} bytes ({file_size/1024/1024:.2f} MB)")
    except:
        file_size = 0
    
    print()
    print("Starting conversion...")
    print()
    
    parser = RESPParser()
    serializer = RESPBSerializer()
    
    total_resp = 0
    total_respb = 0
    command_count = 0
    error_count = 0
    bytes_read = 0
    start_time = time.time()
    last_update = start_time
    
    try:
        with open(input_file, 'rb') as f_in, open(output_file, 'wb') as f_out:
            buffer = b""
            chunk_size = 1024 * 1024  # Read 1MB at a time
            
            while True:
                # Read more data
                chunk = f_in.read(chunk_size)
                if not chunk and not buffer:
                    break  # EOF and buffer empty
                
                buffer += chunk
                bytes_read += len(chunk)
                
                # Try to parse commands from buffer
                while buffer:
                    # Skip any non-command data at the start
                    if buffer and buffer[0:1] != b'*':
                        # Find next command start
                        next_cmd = buffer.find(b'*')
                        if next_cmd == -1:
                            # No more commands in buffer
                            buffer = b""
                            break
                        else:
                            # Skip to next command
                            buffer = buffer[next_cmd:]
                            continue
                    
                    if not buffer.startswith(b'*'):
                        break
                    
                    try:
                        # Try to parse a command
                        elements, consumed = parser.parse_array(buffer, 0)
                        if not elements:
                            break
                        
                        # Calculate actual bytes consumed for this command
                        resp_size = consumed
                        
                        # Build command object
                        command = RESPCommand(
                            command=elements[0].decode('utf-8').upper(),
                            args=elements[1:]
                        )
                        
                        # Convert to RESPB
                        respb_data = serializer.serialize(command)
                        respb_size = len(respb_data)
                        
                        # Write RESPB to output
                        f_out.write(respb_data)
                        
                        # Track stats
                        total_resp += resp_size
                        total_respb += respb_size
                        command_count += 1
                        
                        # Update progress every 50k commands or every 2 seconds
                        current_time = time.time()
                        if command_count % 50000 == 0 or (current_time - last_update) >= 2:
                            elapsed = current_time - start_time
                            cmd_per_sec = command_count / elapsed if elapsed > 0 else 0
                            savings_pct = ((total_resp - total_respb) / total_resp * 100) if total_resp > 0 else 0
                            
                            status = f"[{command_count:,} cmds | {cmd_per_sec:.0f} cmd/s | "
                            status += f"{total_resp/1024/1024:.1f}MB -> {total_respb/1024/1024:.1f}MB | "
                            status += f"Saving {savings_pct:.1f}%"
                            
                            if file_size > 0:
                                progress_pct = (bytes_read / file_size) * 100
                                status += f" | {progress_pct:.1f}% done"
                            
                            status += "]"
                            
                            print(status, end='\r')
                            last_update = current_time
                        
                        # Remove consumed bytes from buffer
                        buffer = buffer[consumed:]
                        
                    except ValueError as e:
                        # Incomplete command, need more data
                        if not chunk:
                            # EOF but incomplete command
                            error_count += 1
                            if error_count <= 10:
                                print(f"\nWarning: Incomplete command at EOF")
                            buffer = b""
                        break  # Read more data
                    except Exception as e:
                        # Error processing command, skip it
                        error_count += 1
                        if error_count <= 10:
                            print(f"\nWarning: Error processing command #{command_count + 1}: {e}")
                        # Try to skip to next command
                        next_cmd = buffer.find(b'*', 1)
                        if next_cmd == -1:
                            buffer = b""
                            break
                        else:
                            buffer = buffer[next_cmd:]
                
                # If we didn't read anything, we're done
                if not chunk:
                    break
        
        # Print final summary
        elapsed = time.time() - start_time
        print(f"\n\n{'='*70}")
        print("CONVERSION COMPLETE")
        print(f"{'='*70}")
        print(f"Time elapsed:       {elapsed:.1f} seconds")
        print(f"Commands processed: {command_count:,} ({command_count/elapsed:.0f} cmd/s)")
        if error_count > 0:
            print(f"Errors encountered: {error_count:,}")
        print()
        print(f"Total RESP size:    {total_resp:,} bytes ({total_resp/1024/1024:.2f} MB)")
        print(f"Total RESPB size:   {total_respb:,} bytes ({total_respb/1024/1024:.2f} MB)")
        print(f"Total savings:      {total_resp - total_respb:,} bytes ({(total_resp-total_respb)/total_resp*100:.1f}%)")
        print()
        if command_count > 0:
            print(f"Average RESP:       {total_resp/command_count:.1f} bytes/command")
            print(f"Average RESPB:      {total_respb/command_count:.1f} bytes/command")
            print(f"Average savings:    {(total_resp-total_respb)/command_count:.1f} bytes/command")
        print(f"\n✓ Output written to: {output_file}")
        print()
        
    except FileNotFoundError as e:
        print(f"ERROR: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"ERROR: Unexpected error: {e}")
        sys.exit(1)


def main():
    """Main entry point with command line argument parsing."""
    parser = argparse.ArgumentParser(
        description='Convert RESP protocol to RESPB binary format',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Run demo with hardcoded test commands
  python3 respb_converter.py
  
  # Convert AOF file
  python3 respb_converter.py -i mendeley/appendonly.aof -o mendeley/appendonly.respb
        """
    )
    
    parser.add_argument('-i', '--input', 
                       help='Input RESP AOF file',
                       default=None)
    parser.add_argument('-o', '--output',
                       help='Output RESPB binary file',
                       default=None)
    
    args = parser.parse_args()
    
    # If no arguments provided, run demo
    if args.input is None and args.output is None:
        run_demo()
    elif args.input and args.output:
        convert_aof_file(args.input, args.output)
    else:
        parser.error("Both --input and --output are required for file conversion")


if __name__ == '__main__':
    main()

