/*
 * RESPB Serializer Implementation
 * Converts parsed commands into binary RESPB format
 */

#include "respb.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void respb_write_u16(uint8_t *buf, uint16_t val) {
    buf[0] = (val >> 8) & 0xFF;
    buf[1] = val & 0xFF;
}

void respb_write_u32(uint8_t *buf, uint32_t val) {
    buf[0] = (val >> 24) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[2] = (val >> 8) & 0xFF;
    buf[3] = val & 0xFF;
}

void respb_write_u64(uint8_t *buf, uint64_t val) {
    buf[0] = (val >> 56) & 0xFF;
    buf[1] = (val >> 48) & 0xFF;
    buf[2] = (val >> 40) & 0xFF;
    buf[3] = (val >> 32) & 0xFF;
    buf[4] = (val >> 24) & 0xFF;
    buf[5] = (val >> 16) & 0xFF;
    buf[6] = (val >> 8) & 0xFF;
    buf[7] = val & 0xFF;
}

size_t respb_serialize_header(uint8_t *buf, uint16_t opcode, uint16_t mux_id) {
    respb_write_u16(buf, opcode);
    respb_write_u16(buf + 2, mux_id);
    return 4;
}

size_t respb_serialize_module_header(uint8_t *buf, uint16_t mux_id, uint32_t subcommand) {
    respb_write_u16(buf, RESPB_OP_MODULE);
    respb_write_u16(buf + 2, mux_id);
    respb_write_u32(buf + 4, subcommand);
    return 8;
}

size_t respb_serialize_command(uint8_t *buf, size_t buf_len, const respb_command_t *cmd) {
    if (buf_len < 4) return 0; // Need at least header space
    
    size_t pos = 0;
    
    // Write header
    pos += respb_serialize_header(buf + pos, cmd->opcode, cmd->mux_id);
    
    // For simplicity, use a generic serialization format for all commands
    // In production, you'd have optimized paths per command type
    switch (cmd->opcode) {
        case RESPB_OP_GET:
        case RESPB_OP_INCR:
        case RESPB_OP_DECR:
        case RESPB_OP_TTL:
        case RESPB_OP_LLEN:
        case RESPB_OP_SCARD: {
            // Single key: [2B keylen][key]
            if (cmd->argc < 1) return 0;
            if (pos + 2 + cmd->args[0].len > buf_len) return 0;
            
            respb_write_u16(buf + pos, cmd->args[0].len);
            pos += 2;
            memcpy(buf + pos, cmd->args[0].data, cmd->args[0].len);
            pos += cmd->args[0].len;
            break;
        }
        
        case RESPB_OP_SET: {
            // [2B keylen][key][4B vallen][value][1B flags][8B expiry]
            if (cmd->argc < 2) return 0;
            if (pos + 2 + cmd->args[0].len + 4 + cmd->args[1].len + 9 > buf_len) return 0;
            
            respb_write_u16(buf + pos, cmd->args[0].len);
            pos += 2;
            memcpy(buf + pos, cmd->args[0].data, cmd->args[0].len);
            pos += cmd->args[0].len;
            
            respb_write_u32(buf + pos, cmd->args[1].len);
            pos += 4;
            memcpy(buf + pos, cmd->args[1].data, cmd->args[1].len);
            pos += cmd->args[1].len;
            
            // Flags and expiry (defaults)
            buf[pos++] = 0; // No flags
            respb_write_u64(buf + pos, 0); // No expiry
            pos += 8;
            break;
        }
        
        case RESPB_OP_APPEND: {
            // [2B keylen][key][4B vallen][value]
            if (cmd->argc < 2) return 0;
            if (pos + 2 + cmd->args[0].len + 4 + cmd->args[1].len > buf_len) return 0;
            
            respb_write_u16(buf + pos, cmd->args[0].len);
            pos += 2;
            memcpy(buf + pos, cmd->args[0].data, cmd->args[0].len);
            pos += cmd->args[0].len;
            
            respb_write_u32(buf + pos, cmd->args[1].len);
            pos += 4;
            memcpy(buf + pos, cmd->args[1].data, cmd->args[1].len);
            pos += cmd->args[1].len;
            break;
        }
        
        case RESPB_OP_INCRBY:
        case RESPB_OP_DECRBY: {
            // [2B keylen][key][8B increment]
            if (cmd->argc < 1) return 0;
            if (pos + 2 + cmd->args[0].len + 8 > buf_len) return 0;
            
            respb_write_u16(buf + pos, cmd->args[0].len);
            pos += 2;
            memcpy(buf + pos, cmd->args[0].data, cmd->args[0].len);
            pos += cmd->args[0].len;
            
            // Default increment of 1
            respb_write_u64(buf + pos, 1);
            pos += 8;
            break;
        }
        
        case RESPB_OP_MGET:
        case RESPB_OP_DEL:
        case RESPB_OP_EXISTS: {
            // [2B count][ [2B keylen][key] ... ]
            if (pos + 2 > buf_len) return 0;
            respb_write_u16(buf + pos, cmd->argc);
            pos += 2;
            
            for (size_t i = 0; i < cmd->argc; i++) {
                if (pos + 2 + cmd->args[i].len > buf_len) return 0;
                respb_write_u16(buf + pos, cmd->args[i].len);
                pos += 2;
                memcpy(buf + pos, cmd->args[i].data, cmd->args[i].len);
                pos += cmd->args[i].len;
            }
            break;
        }
        
        case RESPB_OP_MSET: {
            // [2B npairs][ [2B keylen][key][4B vallen][value] ... ]
            if (cmd->argc < 2 || cmd->argc % 2 != 0) return 0;
            if (pos + 2 > buf_len) return 0;
            
            uint16_t npairs = cmd->argc / 2;
            respb_write_u16(buf + pos, npairs);
            pos += 2;
            
            for (size_t i = 0; i < cmd->argc; i += 2) {
                if (pos + 2 + cmd->args[i].len + 4 + cmd->args[i + 1].len > buf_len) return 0;
                
                respb_write_u16(buf + pos, cmd->args[i].len);
                pos += 2;
                memcpy(buf + pos, cmd->args[i].data, cmd->args[i].len);
                pos += cmd->args[i].len;
                
                respb_write_u32(buf + pos, cmd->args[i + 1].len);
                pos += 4;
                memcpy(buf + pos, cmd->args[i + 1].data, cmd->args[i + 1].len);
                pos += cmd->args[i + 1].len;
            }
            break;
        }
        
        case RESPB_OP_LPUSH:
        case RESPB_OP_RPUSH: {
            // [2B keylen][key][2B count][ [2B elemlen][element] ... ]
            if (cmd->argc < 1) return 0;
            if (pos + 2 + cmd->args[0].len + 2 > buf_len) return 0;
            
            respb_write_u16(buf + pos, cmd->args[0].len);
            pos += 2;
            memcpy(buf + pos, cmd->args[0].data, cmd->args[0].len);
            pos += cmd->args[0].len;
            
            uint16_t count = cmd->argc - 1;
            respb_write_u16(buf + pos, count);
            pos += 2;
            
            for (size_t i = 1; i < cmd->argc; i++) {
                if (pos + 2 + cmd->args[i].len > buf_len) return 0;
                respb_write_u16(buf + pos, cmd->args[i].len);
                pos += 2;
                memcpy(buf + pos, cmd->args[i].data, cmd->args[i].len);
                pos += cmd->args[i].len;
            }
            break;
        }
        
        case RESPB_OP_SADD: {
            // [2B keylen][key][2B count][ [2B memberlen][member] ... ]
            if (cmd->argc < 1) return 0;
            if (pos + 2 + cmd->args[0].len + 2 > buf_len) return 0;
            
            respb_write_u16(buf + pos, cmd->args[0].len);
            pos += 2;
            memcpy(buf + pos, cmd->args[0].data, cmd->args[0].len);
            pos += cmd->args[0].len;
            
            uint16_t count = cmd->argc - 1;
            respb_write_u16(buf + pos, count);
            pos += 2;
            
            for (size_t i = 1; i < cmd->argc; i++) {
                if (pos + 2 + cmd->args[i].len > buf_len) return 0;
                respb_write_u16(buf + pos, cmd->args[i].len);
                pos += 2;
                memcpy(buf + pos, cmd->args[i].data, cmd->args[i].len);
                pos += cmd->args[i].len;
            }
            break;
        }
        
        case RESPB_OP_HSET: {
            // [2B keylen][key][2B npairs][ [2B fieldlen][field][4B vallen][value] ... ]
            if (cmd->argc < 1 || (cmd->argc - 1) % 2 != 0) return 0;
            if (pos + 2 + cmd->args[0].len + 2 > buf_len) return 0;
            
            respb_write_u16(buf + pos, cmd->args[0].len);
            pos += 2;
            memcpy(buf + pos, cmd->args[0].data, cmd->args[0].len);
            pos += cmd->args[0].len;
            
            uint16_t npairs = (cmd->argc - 1) / 2;
            respb_write_u16(buf + pos, npairs);
            pos += 2;
            
            for (size_t i = 1; i < cmd->argc; i += 2) {
                if (pos + 2 + cmd->args[i].len + 4 + cmd->args[i + 1].len > buf_len) return 0;
                
                respb_write_u16(buf + pos, cmd->args[i].len);
                pos += 2;
                memcpy(buf + pos, cmd->args[i].data, cmd->args[i].len);
                pos += cmd->args[i].len;
                
                respb_write_u32(buf + pos, cmd->args[i + 1].len);
                pos += 4;
                memcpy(buf + pos, cmd->args[i + 1].data, cmd->args[i + 1].len);
                pos += cmd->args[i + 1].len;
            }
            break;
        }
        
        case RESPB_OP_HGET: {
            // [2B keylen][key][2B fieldlen][field]
            if (cmd->argc < 2) return 0;
            if (pos + 2 + cmd->args[0].len + 2 + cmd->args[1].len > buf_len) return 0;
            
            respb_write_u16(buf + pos, cmd->args[0].len);
            pos += 2;
            memcpy(buf + pos, cmd->args[0].data, cmd->args[0].len);
            pos += cmd->args[0].len;
            
            respb_write_u16(buf + pos, cmd->args[1].len);
            pos += 2;
            memcpy(buf + pos, cmd->args[1].data, cmd->args[1].len);
            pos += cmd->args[1].len;
            break;
        }
        
        case RESPB_OP_PING:
        case RESPB_OP_MULTI:
        case RESPB_OP_EXEC:
            // No payload
            break;
        
        case RESPB_OP_MODULE: {
            // Module command: 8-byte header with 4-byte subcommand
            if (buf_len < 8) return 0;
            
            // Rewrite header with module opcode and subcommand
            pos = 0;
            pos += respb_serialize_module_header(buf + pos, cmd->mux_id, cmd->module_subcommand);
            
            // Serialize module-specific payload based on module_id and command_id
            if (cmd->module_id == RESPB_MODULE_JSON) {
                // JSON.SET: key + path + json + flags
                if (cmd->command_id == 0x0000 && cmd->argc >= 3) {
                    if (pos + 2 + cmd->args[0].len + 2 + cmd->args[1].len + 4 + cmd->args[2].len + 1 > buf_len) return 0;
                    
                    respb_write_u16(buf + pos, cmd->args[0].len);
                    pos += 2;
                    memcpy(buf + pos, cmd->args[0].data, cmd->args[0].len);
                    pos += cmd->args[0].len;
                    
                    respb_write_u16(buf + pos, cmd->args[1].len);
                    pos += 2;
                    memcpy(buf + pos, cmd->args[1].data, cmd->args[1].len);
                    pos += cmd->args[1].len;
                    
                    respb_write_u32(buf + pos, cmd->args[2].len);
                    pos += 4;
                    memcpy(buf + pos, cmd->args[2].data, cmd->args[2].len);
                    pos += cmd->args[2].len;
                    
                    buf[pos++] = 0; // Flags
                } else {
                    // Generic JSON command serialization
                    for (size_t i = 0; i < cmd->argc; i++) {
                        if (pos + 2 + cmd->args[i].len > buf_len) return 0;
                        respb_write_u16(buf + pos, cmd->args[i].len);
                        pos += 2;
                        memcpy(buf + pos, cmd->args[i].data, cmd->args[i].len);
                        pos += cmd->args[i].len;
                    }
                }
            } else if (cmd->module_id == RESPB_MODULE_BF) {
                // BF.ADD: key + item
                if (cmd->command_id == 0x0000 && cmd->argc >= 2) {
                    if (pos + 2 + cmd->args[0].len + 2 + cmd->args[1].len > buf_len) return 0;
                    
                    respb_write_u16(buf + pos, cmd->args[0].len);
                    pos += 2;
                    memcpy(buf + pos, cmd->args[0].data, cmd->args[0].len);
                    pos += cmd->args[0].len;
                    
                    respb_write_u16(buf + pos, cmd->args[1].len);
                    pos += 2;
                    memcpy(buf + pos, cmd->args[1].data, cmd->args[1].len);
                    pos += cmd->args[1].len;
                } else {
                    // Generic BF command serialization
                    for (size_t i = 0; i < cmd->argc; i++) {
                        if (pos + 2 + cmd->args[i].len > buf_len) return 0;
                        respb_write_u16(buf + pos, cmd->args[i].len);
                        pos += 2;
                        memcpy(buf + pos, cmd->args[i].data, cmd->args[i].len);
                        pos += cmd->args[i].len;
                    }
                }
            } else if (cmd->module_id == RESPB_MODULE_FT) {
                // FT.SEARCH: index + query
                if (cmd->command_id == 0x0001 && cmd->argc >= 2) {
                    if (pos + 2 + cmd->args[0].len + 2 + cmd->args[1].len > buf_len) return 0;
                    
                    respb_write_u16(buf + pos, cmd->args[0].len);
                    pos += 2;
                    memcpy(buf + pos, cmd->args[0].data, cmd->args[0].len);
                    pos += cmd->args[0].len;
                    
                    respb_write_u16(buf + pos, cmd->args[1].len);
                    pos += 2;
                    memcpy(buf + pos, cmd->args[1].data, cmd->args[1].len);
                    pos += cmd->args[1].len;
                } else {
                    // Generic FT command serialization
                    for (size_t i = 0; i < cmd->argc; i++) {
                        if (pos + 2 + cmd->args[i].len > buf_len) return 0;
                        respb_write_u16(buf + pos, cmd->args[i].len);
                        pos += 2;
                        memcpy(buf + pos, cmd->args[i].data, cmd->args[i].len);
                        pos += cmd->args[i].len;
                    }
                }
            } else {
                // Unknown module - generic serialization
                for (size_t i = 0; i < cmd->argc; i++) {
                    if (pos + 2 + cmd->args[i].len > buf_len) return 0;
                    respb_write_u16(buf + pos, cmd->args[i].len);
                    pos += 2;
                    memcpy(buf + pos, cmd->args[i].data, cmd->args[i].len);
                    pos += cmd->args[i].len;
                }
            }
            break;
        }
        
        case RESPB_OP_RESP_PASSTHROUGH: {
            // RESP passthrough: 8-byte header with RESP text data
            if (buf_len < 8) return 0;
            
            // Rewrite header with RESP passthrough opcode
            pos = 0;
            respb_write_u16(buf + pos, RESPB_OP_RESP_PASSTHROUGH);
            pos += 2;
            respb_write_u16(buf + pos, cmd->mux_id);
            pos += 2;
            respb_write_u32(buf + pos, cmd->resp_length);
            pos += 4;
            
            // Copy RESP text data
            if (pos + cmd->resp_length > buf_len) return 0;
            memcpy(buf + pos, cmd->resp_data, cmd->resp_length);
            pos += cmd->resp_length;
            break;
        }
        
        default:
            // Unknown command - use RESP passthrough format
            // This should not happen in normal operation, but provides fallback
            if (pos + 2 > buf_len) return 0;
            respb_write_u16(buf + pos, cmd->argc);
            pos += 2;
            
            for (size_t i = 0; i < cmd->argc; i++) {
                if (pos + 2 + cmd->args[i].len > buf_len) return 0;
                respb_write_u16(buf + pos, cmd->args[i].len);
                pos += 2;
                memcpy(buf + pos, cmd->args[i].data, cmd->args[i].len);
                pos += cmd->args[i].len;
            }
            break;
    }
    
    return pos;
}

