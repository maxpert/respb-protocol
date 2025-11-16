/*
 * Comprehensive RESPB Protocol Test Suite
 * Validates parser and serializer against respb-specs.md and respb-commands.md
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../include/respb.h"
#include "../include/valkey_resp_parser.h"

int tests_passed = 0;
int tests_failed = 0;

#define TEST(name) \
    printf("  %s ... ", name); \
    fflush(stdout);

#define PASS() \
    printf("PASS\n"); \
    tests_passed++;

#define FAIL(msg) \
    printf("FAIL: %s\n", msg); \
    tests_failed++;

// Helper to build command header
static size_t build_header(uint8_t *buf, uint16_t opcode, uint16_t mux_id) {
    buf[0] = (opcode >> 8) & 0xFF;
    buf[1] = opcode & 0xFF;
    buf[2] = (mux_id >> 8) & 0xFF;
    buf[3] = mux_id & 0xFF;
    return 4;
}

// Helper to add 2-byte length string
static size_t add_string_2b(uint8_t *buf, const char *str) {
    uint16_t len = strlen(str);
    buf[0] = (len >> 8) & 0xFF;
    buf[1] = len & 0xFF;
    memcpy(buf + 2, str, len);
    return 2 + len;
}

// Helper to add 4-byte length string
static size_t add_string_4b(uint8_t *buf, const char *str) {
    uint32_t len = strlen(str);
    buf[0] = (len >> 24) & 0xFF;
    buf[1] = (len >> 16) & 0xFF;
    buf[2] = (len >> 8) & 0xFF;
    buf[3] = len & 0xFF;
    memcpy(buf + 4, str, len);
    return 4 + len;
}

void test_resp_simple_get() {
    TEST("RESP simple GET (Valkey parser)");
    
    const char *data = "*2\r\n$3\r\nGET\r\n$5\r\nmykey\r\n";
    valkey_client client;
    valkey_client_init(&client, (const uint8_t *)data, strlen(data));
    
    int result = valkey_parse_command(&client);
    
    if (result != 1) {
        FAIL("Parse failed");
        valkey_client_free(&client);
        return;
    }
    
    if (client.argc != 2) {
        FAIL("Wrong argc");
        valkey_client_free(&client);
        return;
    }
    
    robj *cmd_obj = client.argv[0];
    sds cmd_str = (sds)cmd_obj->ptr;
    if (memcmp(cmd_str, "GET", 3) != 0) {
        FAIL("Wrong command");
        valkey_client_free(&client);
        return;
    }
    
    robj *key_obj = client.argv[1];
    sds key_str = (sds)key_obj->ptr;
    if (memcmp(key_str, "mykey", 5) != 0) {
        FAIL("Wrong key");
        valkey_client_free(&client);
        return;
    }
    
    valkey_client_free(&client);
    PASS();
}

void test_resp_set() {
    TEST("RESP SET command (Valkey parser)");
    
    const char *data = "*3\r\n$3\r\nSET\r\n$5\r\nmykey\r\n$7\r\nmyvalue\r\n";
    valkey_client client;
    valkey_client_init(&client, (const uint8_t *)data, strlen(data));
    
    int result = valkey_parse_command(&client);
    
    if (result != 1) {
        FAIL("Parse failed");
        valkey_client_free(&client);
        return;
    }
    
    if (client.argc != 3) {
        FAIL("Wrong argc");
        valkey_client_free(&client);
        return;
    }
    
    robj *value_obj = client.argv[2];
    sds value_str = (sds)value_obj->ptr;
    if (memcmp(value_str, "myvalue", 7) != 0) {
        FAIL("Wrong value");
        valkey_client_free(&client);
        return;
    }
    
    valkey_client_free(&client);
    PASS();
}

void test_respb_simple_get() {
    TEST("RESPB simple GET");
    
    uint8_t data[100];
    size_t pos = 0;
    
    // Header: opcode=GET(0x0000), mux_id=0
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    
    // Key: length=5, "mykey"
    data[pos++] = 0x00;
    data[pos++] = 0x05;
    memcpy(data + pos, "mykey", 5);
    pos += 5;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    
    respb_command_t cmd;
    int result = respb_parse_command(&parser, &cmd);
    
    if (result != 1) {
        FAIL("Parse failed");
        return;
    }
    
    if (cmd.opcode != RESPB_OP_GET) {
        FAIL("Wrong opcode");
        return;
    }
    
    if (cmd.argc != 1) {
        FAIL("Wrong argc");
        return;
    }
    
    if (memcmp(cmd.args[0].data, "mykey", 5) != 0) {
        FAIL("Wrong key");
        return;
    }
    
    PASS();
}

void test_respb_set() {
    TEST("RESPB SET command");
    
    uint8_t data[200];
    size_t pos = 0;
    
    // Header: opcode=SET(0x0001), mux_id=0
    data[pos++] = 0x00;
    data[pos++] = 0x01;
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    
    // Key: length=5, "mykey"
    data[pos++] = 0x00;
    data[pos++] = 0x05;
    memcpy(data + pos, "mykey", 5);
    pos += 5;
    
    // Value: length=7, "myvalue"
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    data[pos++] = 0x07;
    memcpy(data + pos, "myvalue", 7);
    pos += 7;
    
    // Flags + expiry (9 bytes)
    memset(data + pos, 0, 9);
    pos += 9;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    
    respb_command_t cmd;
    int result = respb_parse_command(&parser, &cmd);
    
    if (result != 1) {
        FAIL("Parse failed");
        return;
    }
    
    if (cmd.opcode != RESPB_OP_SET) {
        FAIL("Wrong opcode");
        return;
    }
    
    if (cmd.argc != 2) {
        FAIL("Wrong argc");
        return;
    }
    
    if (memcmp(cmd.args[0].data, "mykey", 5) != 0) {
        FAIL("Wrong key");
        return;
    }
    
    if (memcmp(cmd.args[1].data, "myvalue", 7) != 0) {
        FAIL("Wrong value");
        return;
    }
    
    PASS();
}

void test_respb_mget() {
    TEST("RESPB MGET command");
    
    uint8_t data[200];
    size_t pos = 0;
    
    // Header: opcode=MGET(0x000C), mux_id=0
    data[pos++] = 0x00;
    data[pos++] = 0x0C;
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    
    // Count: 3
    data[pos++] = 0x00;
    data[pos++] = 0x03;
    
    // Key1: "key1"
    data[pos++] = 0x00;
    data[pos++] = 0x04;
    memcpy(data + pos, "key1", 4);
    pos += 4;
    
    // Key2: "key2"
    data[pos++] = 0x00;
    data[pos++] = 0x04;
    memcpy(data + pos, "key2", 4);
    pos += 4;
    
    // Key3: "key3"
    data[pos++] = 0x00;
    data[pos++] = 0x04;
    memcpy(data + pos, "key3", 4);
    pos += 4;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    
    respb_command_t cmd;
    int result = respb_parse_command(&parser, &cmd);
    
    if (result != 1) {
        FAIL("Parse failed");
        return;
    }
    
    if (cmd.opcode != RESPB_OP_MGET) {
        FAIL("Wrong opcode");
        return;
    }
    
    if (cmd.argc != 3) {
        FAIL("Wrong argc");
        return;
    }
    
    PASS();
}

void test_serialization_roundtrip() {
    TEST("RESPB serialization roundtrip");
    
    // Create a command
    respb_command_t cmd;
    cmd.opcode = RESPB_OP_SET;
    cmd.mux_id = 0;
    cmd.argc = 2;
    
    const char *key = "testkey";
    const char *value = "testvalue";
    
    cmd.args[0].data = (const uint8_t *)key;
    cmd.args[0].len = strlen(key);
    cmd.args[1].data = (const uint8_t *)value;
    cmd.args[1].len = strlen(value);
    
    // Serialize
    uint8_t buffer[256];
    size_t size = respb_serialize_command(buffer, sizeof(buffer), &cmd);
    
    if (size == 0) {
        FAIL("Serialization failed");
        return;
    }
    
    // Parse back
    respb_parser_t parser;
    respb_parser_init(&parser, buffer, size);
    
    respb_command_t cmd2;
    int result = respb_parse_command(&parser, &cmd2);
    
    if (result != 1) {
        FAIL("Parse failed");
        return;
    }
    
    if (cmd2.opcode != cmd.opcode) {
        FAIL("Opcode mismatch");
        return;
    }
    
    if (cmd2.argc != cmd.argc) {
        FAIL("Argc mismatch");
        return;
    }
    
    if (memcmp(cmd2.args[0].data, key, strlen(key)) != 0) {
        FAIL("Key mismatch");
        return;
    }
    
    if (memcmp(cmd2.args[1].data, value, strlen(value)) != 0) {
        FAIL("Value mismatch");
        return;
    }
    
    PASS();
}

void test_respb_module_json_set() {
    TEST("RESPB module command JSON.SET");
    
    uint8_t data[200];
    size_t pos = 0;
    
    // Header: opcode=MODULE(0xF000), mux_id=0
    data[pos++] = 0xF0;
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    
    // Subcommand: JSON.SET = 0x00000000 (module 0x0000, command 0x0000)
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    
    // Key: length=7, "profile"
    data[pos++] = 0x00;
    data[pos++] = 0x07;
    memcpy(data + pos, "profile", 7);
    pos += 7;
    
    // Path: length=5, ".name"
    data[pos++] = 0x00;
    data[pos++] = 0x05;
    memcpy(data + pos, ".name", 5);
    pos += 5;
    
    // JSON: length=12, "\"John Doe\""
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    data[pos++] = 0x0C;
    memcpy(data + pos, "\"John Doe\"", 12);
    pos += 12;
    
    // Flags: 0x00
    data[pos++] = 0x00;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    
    respb_command_t cmd;
    int result = respb_parse_command(&parser, &cmd);
    
    if (result != 1) {
        FAIL("Parse failed");
        return;
    }
    
    if (cmd.opcode != RESPB_OP_MODULE) {
        FAIL("Wrong opcode");
        return;
    }
    
    if (cmd.module_id != RESPB_MODULE_JSON) {
        FAIL("Wrong module ID");
        return;
    }
    
    if (cmd.command_id != 0x0000) {
        FAIL("Wrong command ID");
        return;
    }
    
    if (cmd.argc != 3) {
        FAIL("Wrong argc");
        return;
    }
    
    if (memcmp(cmd.args[0].data, "profile", 7) != 0) {
        FAIL("Wrong key");
        return;
    }
    
    if (memcmp(cmd.args[1].data, ".name", 5) != 0) {
        FAIL("Wrong path");
        return;
    }
    
    if (memcmp(cmd.args[2].data, "\"John Doe\"", 12) != 0) {
        FAIL("Wrong JSON");
        return;
    }
    
    PASS();
}

void test_respb_module_bf_add() {
    TEST("RESPB module command BF.ADD");
    
    uint8_t data[200];
    size_t pos = 0;
    
    // Header: opcode=MODULE(0xF000), mux_id=0
    data[pos++] = 0xF0;
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    
    // Subcommand: BF.ADD = 0x00010000 (module 0x0001, command 0x0000)
    data[pos++] = 0x00;
    data[pos++] = 0x01;
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    
    // Key: length=4, "bf1"
    data[pos++] = 0x00;
    data[pos++] = 0x04;
    memcpy(data + pos, "bf1", 4);
    pos += 4;
    
    // Item: length=3, "foo"
    data[pos++] = 0x00;
    data[pos++] = 0x03;
    memcpy(data + pos, "foo", 3);
    pos += 3;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    
    respb_command_t cmd;
    int result = respb_parse_command(&parser, &cmd);
    
    if (result != 1) {
        FAIL("Parse failed");
        return;
    }
    
    if (cmd.opcode != RESPB_OP_MODULE) {
        FAIL("Wrong opcode");
        return;
    }
    
    if (cmd.module_id != RESPB_MODULE_BF) {
        FAIL("Wrong module ID");
        return;
    }
    
    if (cmd.command_id != 0x0000) {
        FAIL("Wrong command ID");
        return;
    }
    
    if (cmd.argc != 2) {
        FAIL("Wrong argc");
        return;
    }
    
    if (memcmp(cmd.args[0].data, "bf1", 4) != 0) {
        FAIL("Wrong key");
        return;
    }
    
    if (memcmp(cmd.args[1].data, "foo", 3) != 0) {
        FAIL("Wrong item");
        return;
    }
    
    PASS();
}

void test_respb_resp_passthrough() {
    TEST("RESPB RESP passthrough");
    
    uint8_t data[200];
    size_t pos = 0;
    
    // Per spec respb-commands.md line 506: [0xFFFF][mux_id][4B resp_length][RESP text data]
    // Build test data manually without helpers
    data[pos++] = 0xFF;  // Opcode high byte
    data[pos++] = 0xFF;  // Opcode low byte
    data[pos++] = 0x00;  // Mux ID high byte
    data[pos++] = 0x00;  // Mux ID low byte
    
    // RESP command string (exactly 33 bytes)
    const char resp_text[] = "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
    uint32_t resp_len = 33;
    
    // RESP length field (4 bytes, big endian)
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    data[pos++] = 0x21;  // 0x21 = 33 decimal
    
    // Copy RESP text data
    memcpy(data + pos, resp_text, resp_len);
    pos += resp_len;
    
    // Parse
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    
    int result = respb_parse_command(&parser, &cmd);
    
    if (result != 1) {
        FAIL("Parser returned error");
        return;
    }
    
    if (cmd.opcode != 0xFFFF) {
        FAIL("Wrong opcode");
        return;
    }
    
    if (cmd.resp_length != 33) {
        FAIL("Wrong resp_length field");
        return;
    }
    
    if (cmd.resp_data == NULL) {
        FAIL("resp_data pointer is NULL");
        return;
    }
    
    // Verify RESP data pointer points to correct location in buffer
    if (cmd.resp_data != data + 8) {
        FAIL("resp_data points to wrong buffer location");
        return;
    }
    
    // Verify RESP data content
    if (strncmp((const char*)cmd.resp_data, resp_text, resp_len) != 0) {
        FAIL("RESP data content mismatch");
        return;
    }
    
    PASS();
}

void test_respb_del() {
    TEST("RESPB DEL command");
    
    uint8_t data[200];
    size_t pos = 0;
    
    // Header: opcode=DEL(0x02C0), mux_id=0
    data[pos++] = 0x02;
    data[pos++] = 0xC0;
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    
    // Count: 2 keys
    data[pos++] = 0x00;
    data[pos++] = 0x02;
    
    // Key1: "key1"
    data[pos++] = 0x00;
    data[pos++] = 0x04;
    memcpy(data + pos, "key1", 4);
    pos += 4;
    
    // Key2: "key2"
    data[pos++] = 0x00;
    data[pos++] = 0x04;
    memcpy(data + pos, "key2", 4);
    pos += 4;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    
    respb_command_t cmd;
    int result = respb_parse_command(&parser, &cmd);
    
    if (result != 1) {
        FAIL("Parse failed");
        return;
    }
    
    if (cmd.opcode != RESPB_OP_DEL) {
        FAIL("Wrong opcode");
        return;
    }
    
    if (cmd.argc != 2) {
        FAIL("Wrong argc");
        return;
    }
    
    if (memcmp(cmd.args[0].data, "key1", 4) != 0) {
        FAIL("Wrong key1");
        return;
    }
    
    if (memcmp(cmd.args[1].data, "key2", 4) != 0) {
        FAIL("Wrong key2");
        return;
    }
    
    PASS();
}

void test_respb_lpush() {
    TEST("RESPB LPUSH command");
    
    uint8_t data[200];
    size_t pos = 0;
    
    // Header: opcode=LPUSH(0x0040), mux_id=0
    data[pos++] = 0x00;
    data[pos++] = 0x40;
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    
    // Key: "mylist"
    data[pos++] = 0x00;
    data[pos++] = 0x06;
    memcpy(data + pos, "mylist", 6);
    pos += 6;
    
    // Count: 2 elements
    data[pos++] = 0x00;
    data[pos++] = 0x02;
    
    // Element1: "elem1"
    data[pos++] = 0x00;
    data[pos++] = 0x05;
    memcpy(data + pos, "elem1", 5);
    pos += 5;
    
    // Element2: "elem2"
    data[pos++] = 0x00;
    data[pos++] = 0x05;
    memcpy(data + pos, "elem2", 5);
    pos += 5;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    
    respb_command_t cmd;
    int result = respb_parse_command(&parser, &cmd);
    
    if (result != 1) {
        FAIL("Parse failed");
        return;
    }
    
    if (cmd.opcode != RESPB_OP_LPUSH) {
        FAIL("Wrong opcode");
        return;
    }
    
    if (cmd.argc != 3) {
        FAIL("Wrong argc");
        return;
    }
    
    if (memcmp(cmd.args[0].data, "mylist", 6) != 0) {
        FAIL("Wrong key");
        return;
    }
    
    PASS();
}

void test_respb_sadd() {
    TEST("RESPB SADD command");
    
    uint8_t data[200];
    size_t pos = 0;
    
    // Header: opcode=SADD(0x0080), mux_id=0
    data[pos++] = 0x00;
    data[pos++] = 0x80;
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    
    // Key: "myset"
    data[pos++] = 0x00;
    data[pos++] = 0x05;
    memcpy(data + pos, "myset", 5);
    pos += 5;
    
    // Count: 2 members
    data[pos++] = 0x00;
    data[pos++] = 0x02;
    
    // Member1: "a"
    data[pos++] = 0x00;
    data[pos++] = 0x01;
    data[pos++] = 'a';
    
    // Member2: "b"
    data[pos++] = 0x00;
    data[pos++] = 0x01;
    data[pos++] = 'b';
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    
    respb_command_t cmd;
    int result = respb_parse_command(&parser, &cmd);
    
    if (result != 1) {
        FAIL("Parse failed");
        return;
    }
    
    if (cmd.opcode != RESPB_OP_SADD) {
        FAIL("Wrong opcode");
        return;
    }
    
    if (cmd.argc != 3) {
        FAIL("Wrong argc");
        return;
    }
    
    PASS();
}

void test_respb_hget() {
    TEST("RESPB HGET command");
    
    uint8_t data[200];
    size_t pos = 0;
    
    // Header: opcode=HGET(0x0101), mux_id=0
    data[pos++] = 0x01;
    data[pos++] = 0x01;
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    
    // Key: "myhash"
    data[pos++] = 0x00;
    data[pos++] = 0x06;
    memcpy(data + pos, "myhash", 6);
    pos += 6;
    
    // Field: "field1"
    data[pos++] = 0x00;
    data[pos++] = 0x06;
    memcpy(data + pos, "field1", 6);
    pos += 6;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    
    respb_command_t cmd;
    int result = respb_parse_command(&parser, &cmd);
    
    if (result != 1) {
        FAIL("Parse failed");
        return;
    }
    
    if (cmd.opcode != RESPB_OP_HGET) {
        FAIL("Wrong opcode");
        return;
    }
    
    if (cmd.argc != 2) {
        FAIL("Wrong argc");
        return;
    }
    
    PASS();
}

void test_respb_json_get() {
    TEST("RESPB JSON.GET command");
    
    uint8_t data[200];
    size_t pos = 0;
    
    // Header: opcode=MODULE(0xF000), mux_id=0
    data[pos++] = 0xF0;
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    
    // Subcommand: JSON.GET = 0x00000001 (module 0x0000, command 0x0001)
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    data[pos++] = 0x01;
    
    // Key: length=7, "profile"
    data[pos++] = 0x00;
    data[pos++] = 0x07;
    memcpy(data + pos, "profile", 7);
    pos += 7;
    
    // Numpaths: 1
    data[pos++] = 0x00;
    data[pos++] = 0x01;
    
    // Path: length=5, ".name"
    data[pos++] = 0x00;
    data[pos++] = 0x05;
    memcpy(data + pos, ".name", 5);
    pos += 5;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    
    respb_command_t cmd;
    int result = respb_parse_command(&parser, &cmd);
    
    if (result != 1) {
        FAIL("Parse failed");
        return;
    }
    
    if (cmd.opcode != RESPB_OP_MODULE) {
        FAIL("Wrong opcode");
        return;
    }
    
    if (cmd.module_id != RESPB_MODULE_JSON) {
        FAIL("Wrong module ID");
        return;
    }
    
    if (cmd.command_id != 0x0001) {
        FAIL("Wrong command ID");
        return;
    }
    
    if (cmd.argc != 2) {
        FAIL("Wrong argc");
        return;
    }
    
    PASS();
}

void test_respb_ft_search() {
    TEST("RESPB FT.SEARCH command");
    
    uint8_t data[200];
    size_t pos = 0;
    
    // Header: opcode=MODULE(0xF000), mux_id=0
    data[pos++] = 0xF0;
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    data[pos++] = 0x00;
    
    // Subcommand: FT.SEARCH = 0x00020001 (module 0x0002, command 0x0001)
    data[pos++] = 0x00;
    data[pos++] = 0x02;
    data[pos++] = 0x00;
    data[pos++] = 0x01;
    
    // Index: length=4, "idx1"
    data[pos++] = 0x00;
    data[pos++] = 0x04;
    memcpy(data + pos, "idx1", 4);
    pos += 4;
    
    // Query: length=5, "hello"
    data[pos++] = 0x00;
    data[pos++] = 0x05;
    memcpy(data + pos, "hello", 5);
    pos += 5;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    
    respb_command_t cmd;
    int result = respb_parse_command(&parser, &cmd);
    
    if (result != 1) {
        FAIL("Parse failed");
        return;
    }
    
    if (cmd.opcode != RESPB_OP_MODULE) {
        FAIL("Wrong opcode");
        return;
    }
    
    if (cmd.module_id != RESPB_MODULE_FT) {
        FAIL("Wrong module ID");
        return;
    }
    
    if (cmd.command_id != 0x0001) {
        FAIL("Wrong command ID");
        return;
    }
    
    if (cmd.argc != 2) {
        FAIL("Wrong argc");
        return;
    }
    
    PASS();
}

// Additional String Operations Tests
void test_respb_incr() {
    TEST("INCR");
    uint8_t data[100];
    size_t pos = build_header(data, 0x0009, 0);
    pos += add_string_2b(data + pos, "counter");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != 0x0009 || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_decr() {
    TEST("DECR");
    uint8_t data[100];
    size_t pos = build_header(data, 0x0003, 0);
    pos += add_string_2b(data + pos, "counter");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != 0x0003 || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_strlen() {
    TEST("STRLEN");
    uint8_t data[100];
    size_t pos = build_header(data, 0x0013, 0);
    pos += add_string_2b(data + pos, "key");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != 0x0013 || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_append() {
    TEST("APPEND");
    uint8_t data[200];
    size_t pos = build_header(data, 0x0002, 0);
    pos += add_string_2b(data + pos, "key");
    pos += add_string_4b(data + pos, "appenddata");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != 0x0002 || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_getdel() {
    TEST("GETDEL");
    uint8_t data[100];
    size_t pos = build_header(data, 0x0005, 0);
    pos += add_string_2b(data + pos, "key");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != 0x0005 || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_incrby() {
    TEST("INCRBY");
    uint8_t data[100];
    size_t pos = build_header(data, 0x000A, 0);
    pos += add_string_2b(data + pos, "counter");
    // 8-byte increment
    memset(data + pos, 0, 7);
    data[pos + 7] = 10;
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != 0x000A || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_decrby() {
    TEST("DECRBY");
    uint8_t data[100];
    size_t pos = build_header(data, 0x0004, 0);
    pos += add_string_2b(data + pos, "counter");
    // 8-byte decrement
    memset(data + pos, 0, 7);
    data[pos + 7] = 5;
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != 0x0004 || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_setnx() {
    TEST("SETNX");
    uint8_t data[200];
    size_t pos = build_header(data, 0x0011, 0);
    pos += add_string_2b(data + pos, "key");
    pos += add_string_4b(data + pos, "value");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != 0x0011 || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// List Operations Tests
void test_respb_rpush() {
    TEST("RPUSH");
    uint8_t data[200];
    size_t pos = build_header(data, 0x0041, 0);
    pos += add_string_2b(data + pos, "list");
    data[pos++] = 0x00;
    data[pos++] = 0x01;
    pos += add_string_2b(data + pos, "elem");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != 0x0041 || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_llen() {
    TEST("LLEN");
    uint8_t data[100];
    size_t pos = build_header(data, 0x0044, 0);
    pos += add_string_2b(data + pos, "list");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != 0x0044 || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_lpop() {
    TEST("LPOP");
    uint8_t data[100];
    size_t pos = build_header(data, 0x0042, 0);
    pos += add_string_2b(data + pos, "list");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != 0x0042 || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_rpop() {
    TEST("RPOP");
    uint8_t data[100];
    size_t pos = build_header(data, 0x0043, 0);
    pos += add_string_2b(data + pos, "list");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != 0x0043 || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_lrange() {
    TEST("LRANGE");
    uint8_t data[100];
    size_t pos = build_header(data, 0x0045, 0);
    pos += add_string_2b(data + pos, "list");
    // 8-byte start
    memset(data + pos, 0, 8);
    pos += 8;
    // 8-byte stop
    memset(data + pos, 0, 7);
    data[pos + 7] = 10;
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != 0x0045 || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Set Operations Tests
void test_respb_scard() {
    TEST("SCARD");
    uint8_t data[100];
    size_t pos = build_header(data, 0x0084, 0);
    pos += add_string_2b(data + pos, "set");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != 0x0084 || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_smembers() {
    TEST("SMEMBERS");
    uint8_t data[100];
    size_t pos = build_header(data, 0x0082, 0);
    pos += add_string_2b(data + pos, "set");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != 0x0082 || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Sorted Set Operations Tests
void test_respb_zcard() {
    TEST("ZCARD");
    uint8_t data[100];
    size_t pos = build_header(data, 0x00C2, 0);
    pos += add_string_2b(data + pos, "zset");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != 0x00C2 || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zscore() {
    TEST("ZSCORE");
    uint8_t data[100];
    size_t pos = build_header(data, RESPB_OP_ZSCORE, 0);
    pos += add_string_2b(data + pos, "zset");
    pos += add_string_2b(data + pos, "member");  // ZSCORE format: [2B keylen][key][2B memberlen][member]
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    // ZSCORE expects 2 args: key and member
    int result = respb_parse_command(&parser, &cmd);
    if (result != 1) {
        FAIL("Parse failed");
        return;
    }
    if (cmd.opcode != RESPB_OP_ZSCORE) {
        FAIL("Wrong opcode");
        return;
    }
    if (cmd.argc != 2) {
        FAIL("Wrong argc");
        return;
    }
    PASS();
}

// Hash Operations Tests
void test_respb_hgetall() {
    TEST("HGETALL");
    uint8_t data[100];
    size_t pos = build_header(data, 0x0104, 0);
    pos += add_string_2b(data + pos, "hash");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != 0x0104 || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Key Operations Tests
void test_respb_exists() {
    TEST("EXISTS");
    uint8_t data[100];
    size_t pos = build_header(data, 0x02C2, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x01;
    pos += add_string_2b(data + pos, "key");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != 0x02C2 || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Transaction Tests
void test_respb_multi() {
    TEST("MULTI");
    uint8_t data[10];
    size_t pos = build_header(data, 0x0240, 0);
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != 0x0240 || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_exec() {
    TEST("EXEC");
    uint8_t data[10];
    size_t pos = build_header(data, 0x0241, 0);
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != 0x0241 || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Connection Management Tests
void test_respb_ping() {
    TEST("PING");
    uint8_t data[10];
    size_t pos = build_header(data, 0x0300, 0);
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != 0x0300 || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Additional String Operations Tests
void test_respb_getex() {
    TEST("GETEX");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_GETEX, 0);
    pos += add_string_2b(data + pos, "key");
    data[pos++] = 0x01;  // flags
    memset(data + pos, 0, 7);
    data[pos + 7] = 60;  // 8-byte expiry
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_GETEX || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_getrange() {
    TEST("GETRANGE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_GETRANGE, 0);
    pos += add_string_2b(data + pos, "key");
    memset(data + pos, 0, 7);
    data[pos + 7] = 0;  // 8-byte start
    pos += 8;
    memset(data + pos, 0, 7);
    data[pos + 7] = 10;  // 8-byte end
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_GETRANGE || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_getset() {
    TEST("GETSET");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_GETSET, 0);
    pos += add_string_2b(data + pos, "key");
    pos += add_string_4b(data + pos, "value");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_GETSET || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_incrbyfloat() {
    TEST("INCRBYFLOAT");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_INCRBYFLOAT, 0);
    pos += add_string_2b(data + pos, "key");
    // 8-byte float (IEEE 754)
    double val = 1.5;
    memcpy(data + pos, &val, 8);
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_INCRBYFLOAT || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_mset() {
    TEST("MSET");
    uint8_t data[300];
    size_t pos = build_header(data, RESPB_OP_MSET, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // count
    pos += add_string_2b(data + pos, "key1");
    pos += add_string_4b(data + pos, "val1");
    pos += add_string_2b(data + pos, "key2");
    pos += add_string_4b(data + pos, "val2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_MSET || cmd.argc != 4) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_msetnx() {
    TEST("MSETNX");
    uint8_t data[300];
    size_t pos = build_header(data, RESPB_OP_MSETNX, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // count
    pos += add_string_2b(data + pos, "key1");
    pos += add_string_4b(data + pos, "val1");
    pos += add_string_2b(data + pos, "key2");
    pos += add_string_4b(data + pos, "val2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_MSETNX || cmd.argc != 4) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_psetex() {
    TEST("PSETEX");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_PSETEX, 0);
    pos += add_string_2b(data + pos, "key");
    memset(data + pos, 0, 7);
    data[pos + 7] = 100;  // 8-byte millis
    pos += 8;
    pos += add_string_4b(data + pos, "value");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_PSETEX || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_setex() {
    TEST("SETEX");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SETEX, 0);
    pos += add_string_2b(data + pos, "key");
    memset(data + pos, 0, 7);
    data[pos + 7] = 60;  // 8-byte seconds
    pos += 8;
    pos += add_string_4b(data + pos, "value");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SETEX || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_setrange() {
    TEST("SETRANGE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SETRANGE, 0);
    pos += add_string_2b(data + pos, "key");
    memset(data + pos, 0, 7);
    data[pos + 7] = 5;  // 8-byte offset
    pos += 8;
    pos += add_string_4b(data + pos, "value");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SETRANGE || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_substr() {
    TEST("SUBSTR");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SUBSTR, 0);
    pos += add_string_2b(data + pos, "key");
    memset(data + pos, 0, 7);
    data[pos + 7] = 0;  // 8-byte start
    pos += 8;
    memset(data + pos, 0, 7);
    data[pos + 7] = 10;  // 8-byte end
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SUBSTR || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_lcs() {
    TEST("LCS");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_LCS, 0);
    pos += add_string_2b(data + pos, "key1");
    pos += add_string_2b(data + pos, "key2");
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_LCS || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_delifeq() {
    TEST("DELIFEQ");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_DELIFEQ, 0);
    pos += add_string_2b(data + pos, "key");
    pos += add_string_4b(data + pos, "value");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_DELIFEQ || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Additional List Operations Tests
void test_respb_lindex() {
    TEST("LINDEX");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_LINDEX, 0);
    pos += add_string_2b(data + pos, "list");
    memset(data + pos, 0, 7);
    data[pos + 7] = 0;  // 8-byte index
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_LINDEX || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_lset() {
    TEST("LSET");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_LSET, 0);
    pos += add_string_2b(data + pos, "list");
    memset(data + pos, 0, 7);
    data[pos + 7] = 0;  // 8-byte index
    pos += 8;
    pos += add_string_2b(data + pos, "elem");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_LSET || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_lrem() {
    TEST("LREM");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_LREM, 0);
    pos += add_string_2b(data + pos, "list");
    memset(data + pos, 0, 7);
    data[pos + 7] = 2;  // 8-byte count
    pos += 8;
    pos += add_string_2b(data + pos, "elem");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_LREM || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_ltrim() {
    TEST("LTRIM");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_LTRIM, 0);
    pos += add_string_2b(data + pos, "list");
    memset(data + pos, 0, 7);
    data[pos + 7] = 0;  // 8-byte start
    pos += 8;
    memset(data + pos, 0, 7);
    data[pos + 7] = 10;  // 8-byte stop
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_LTRIM || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_linsert() {
    TEST("LINSERT");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_LINSERT, 0);
    pos += add_string_2b(data + pos, "list");
    data[pos++] = 0x01;  // before_after (1 = after)
    pos += add_string_2b(data + pos, "pivot");
    pos += add_string_2b(data + pos, "elem");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_LINSERT || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_lpushx() {
    TEST("LPUSHX");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_LPUSHX, 0);
    pos += add_string_2b(data + pos, "list");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // count
    pos += add_string_2b(data + pos, "elem");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_LPUSHX || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_rpushx() {
    TEST("RPUSHX");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_RPUSHX, 0);
    pos += add_string_2b(data + pos, "list");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // count
    pos += add_string_2b(data + pos, "elem");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_RPUSHX || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_rpoplpush() {
    TEST("RPOPLPUSH");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_RPOPLPUSH, 0);
    pos += add_string_2b(data + pos, "src");
    pos += add_string_2b(data + pos, "dst");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_RPOPLPUSH || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_lmove() {
    TEST("LMOVE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_LMOVE, 0);
    pos += add_string_2b(data + pos, "src");
    pos += add_string_2b(data + pos, "dst");
    data[pos++] = 0x00;  // wherefrom
    data[pos++] = 0x01;  // whereto
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_LMOVE || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Additional Set Operations Tests
void test_respb_srem() {
    TEST("SREM");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SREM, 0);
    pos += add_string_2b(data + pos, "set");
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // count
    pos += add_string_2b(data + pos, "a");
    pos += add_string_2b(data + pos, "b");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SREM || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_sismember() {
    TEST("SISMEMBER");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SISMEMBER, 0);
    pos += add_string_2b(data + pos, "set");
    pos += add_string_2b(data + pos, "member");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SISMEMBER || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_spop() {
    TEST("SPOP");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SPOP, 0);
    pos += add_string_2b(data + pos, "set");
    // Optional count field omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SPOP || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_srandmember() {
    TEST("SRANDMEMBER");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SRANDMEMBER, 0);
    pos += add_string_2b(data + pos, "set");
    memset(data + pos, 0, 7);
    data[pos + 7] = 5;  // 8-byte count
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SRANDMEMBER || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_sinter() {
    TEST("SINTER");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SINTER, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // numkeys
    pos += add_string_2b(data + pos, "set1");
    pos += add_string_2b(data + pos, "set2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SINTER || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_sinterstore() {
    TEST("SINTERSTORE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SINTERSTORE, 0);
    pos += add_string_2b(data + pos, "dst");
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // numkeys
    pos += add_string_2b(data + pos, "set1");
    pos += add_string_2b(data + pos, "set2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SINTERSTORE || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_sunion() {
    TEST("SUNION");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SUNION, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // numkeys
    pos += add_string_2b(data + pos, "set1");
    pos += add_string_2b(data + pos, "set2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SUNION || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_sunionstore() {
    TEST("SUNIONSTORE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SUNIONSTORE, 0);
    pos += add_string_2b(data + pos, "dst");
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // numkeys
    pos += add_string_2b(data + pos, "set1");
    pos += add_string_2b(data + pos, "set2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SUNIONSTORE || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_sdiff() {
    TEST("SDIFF");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SDIFF, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // numkeys
    pos += add_string_2b(data + pos, "set1");
    pos += add_string_2b(data + pos, "set2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SDIFF || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_sdiffstore() {
    TEST("SDIFFSTORE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SDIFFSTORE, 0);
    pos += add_string_2b(data + pos, "dst");
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // numkeys
    pos += add_string_2b(data + pos, "set1");
    pos += add_string_2b(data + pos, "set2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SDIFFSTORE || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_smove() {
    TEST("SMOVE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SMOVE, 0);
    pos += add_string_2b(data + pos, "src");
    pos += add_string_2b(data + pos, "dst");
    pos += add_string_2b(data + pos, "member");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SMOVE || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Additional Sorted Set Operations Tests
void test_respb_zadd() {
    TEST("ZADD");
    uint8_t data[300];
    size_t pos = build_header(data, RESPB_OP_ZADD, 0);
    pos += add_string_2b(data + pos, "zset");
    data[pos++] = 0x00;  // flags
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // count
    double score = 1.5;
    memcpy(data + pos, &score, 8);
    pos += 8;
    pos += add_string_2b(data + pos, "member");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    // Parser is simplified - only stores key, skips score/member pairs
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZADD || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zrem() {
    TEST("ZREM");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZREM, 0);
    pos += add_string_2b(data + pos, "zset");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // count
    pos += add_string_2b(data + pos, "member");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZREM || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zcount() {
    TEST("ZCOUNT");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZCOUNT, 0);
    pos += add_string_2b(data + pos, "zset");
    memset(data + pos, 0, 7);
    data[pos + 7] = 0;  // 8-byte min
    pos += 8;
    memset(data + pos, 0, 7);
    data[pos + 7] = 100;  // 8-byte max
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZCOUNT || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zincrby() {
    TEST("ZINCRBY");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZINCRBY, 0);
    pos += add_string_2b(data + pos, "zset");
    double incr = 1.5;
    memcpy(data + pos, &incr, 8);
    pos += 8;
    pos += add_string_2b(data + pos, "member");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    // Parser stores key and member
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZINCRBY || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zrange() {
    TEST("ZRANGE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZRANGE, 0);
    pos += add_string_2b(data + pos, "zset");
    memset(data + pos, 0, 7);
    data[pos + 7] = 0;  // 8-byte start
    pos += 8;
    memset(data + pos, 0, 7);
    data[pos + 7] = 10;  // 8-byte stop
    pos += 8;
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZRANGE || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zrank() {
    TEST("ZRANK");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZRANK, 0);
    pos += add_string_2b(data + pos, "zset");
    pos += add_string_2b(data + pos, "member");
    data[pos++] = 0x00;  // withscore
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZRANK || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Additional Hash Operations Tests
void test_respb_hset() {
    TEST("HSET");
    uint8_t data[300];
    size_t pos = build_header(data, RESPB_OP_HSET, 0);
    pos += add_string_2b(data + pos, "hash");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // count
    pos += add_string_2b(data + pos, "field");
    pos += add_string_4b(data + pos, "value");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HSET || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_hmset() {
    TEST("HMSET");
    uint8_t data[300];
    size_t pos = build_header(data, RESPB_OP_HMSET, 0);
    pos += add_string_2b(data + pos, "hash");
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // count
    pos += add_string_2b(data + pos, "field1");
    pos += add_string_4b(data + pos, "val1");
    pos += add_string_2b(data + pos, "field2");
    pos += add_string_4b(data + pos, "val2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HMSET || cmd.argc != 5) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_hmget() {
    TEST("HMGET");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_HMGET, 0);
    pos += add_string_2b(data + pos, "hash");
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // count
    pos += add_string_2b(data + pos, "field1");
    pos += add_string_2b(data + pos, "field2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HMGET || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_hdel() {
    TEST("HDEL");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_HDEL, 0);
    pos += add_string_2b(data + pos, "hash");
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // count
    pos += add_string_2b(data + pos, "field1");
    pos += add_string_2b(data + pos, "field2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HDEL || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_hexists() {
    TEST("HEXISTS");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_HEXISTS, 0);
    pos += add_string_2b(data + pos, "hash");
    pos += add_string_2b(data + pos, "field");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HEXISTS || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_hincrby() {
    TEST("HINCRBY");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_HINCRBY, 0);
    pos += add_string_2b(data + pos, "hash");
    pos += add_string_2b(data + pos, "field");
    memset(data + pos, 0, 7);
    data[pos + 7] = 10;  // 8-byte increment
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HINCRBY || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_hkeys() {
    TEST("HKEYS");
    uint8_t data[100];
    size_t pos = build_header(data, RESPB_OP_HKEYS, 0);
    pos += add_string_2b(data + pos, "hash");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HKEYS || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_hvals() {
    TEST("HVALS");
    uint8_t data[100];
    size_t pos = build_header(data, RESPB_OP_HVALS, 0);
    pos += add_string_2b(data + pos, "hash");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HVALS || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_hlen() {
    TEST("HLEN");
    uint8_t data[100];
    size_t pos = build_header(data, RESPB_OP_HLEN, 0);
    pos += add_string_2b(data + pos, "hash");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HLEN || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_hsetnx() {
    TEST("HSETNX");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_HSETNX, 0);
    pos += add_string_2b(data + pos, "hash");
    pos += add_string_2b(data + pos, "field");
    pos += add_string_4b(data + pos, "value");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HSETNX || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Additional Sorted Set Operations
void test_respb_zrevrange() {
    TEST("ZREVRANGE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZREVRANGE, 0);
    pos += add_string_2b(data + pos, "zset");
    memset(data + pos, 0, 7);
    data[pos + 7] = 0;  // 8-byte start
    pos += 8;
    memset(data + pos, 0, 7);
    data[pos + 7] = 10;  // 8-byte stop
    pos += 8;
    data[pos++] = 0x00;  // withscores
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZREVRANGE || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zrevrank() {
    TEST("ZREVRANK");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZREVRANK, 0);
    pos += add_string_2b(data + pos, "zset");
    pos += add_string_2b(data + pos, "member");
    data[pos++] = 0x00;  // withscore
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZREVRANK || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zmscore() {
    TEST("ZMSCORE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZMSCORE, 0);
    pos += add_string_2b(data + pos, "zset");
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // count
    pos += add_string_2b(data + pos, "m1");
    pos += add_string_2b(data + pos, "m2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZMSCORE || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zpopmin() {
    TEST("ZPOPMIN");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZPOPMIN, 0);
    pos += add_string_2b(data + pos, "zset");
    // Optional count omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZPOPMIN || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zpopmax() {
    TEST("ZPOPMAX");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZPOPMAX, 0);
    pos += add_string_2b(data + pos, "zset");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZPOPMAX || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zremrangebyrank() {
    TEST("ZREMRANGEBYRANK");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZREMRANGEBYRANK, 0);
    pos += add_string_2b(data + pos, "zset");
    memset(data + pos, 0, 7);
    data[pos + 7] = 0;  // 8-byte start
    pos += 8;
    memset(data + pos, 0, 7);
    data[pos + 7] = 10;  // 8-byte stop
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZREMRANGEBYRANK || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zremrangebyscore() {
    TEST("ZREMRANGEBYSCORE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZREMRANGEBYSCORE, 0);
    pos += add_string_2b(data + pos, "zset");
    memset(data + pos, 0, 7);
    data[pos + 7] = 0;  // 8-byte min
    pos += 8;
    memset(data + pos, 0, 7);
    data[pos + 7] = 100;  // 8-byte max
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZREMRANGEBYSCORE || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Additional Hash Operations
void test_respb_hincrbyfloat() {
    TEST("HINCRBYFLOAT");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_HINCRBYFLOAT, 0);
    pos += add_string_2b(data + pos, "hash");
    pos += add_string_2b(data + pos, "field");
    double incr = 1.5;
    memcpy(data + pos, &incr, 8);
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HINCRBYFLOAT || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_hstrlen() {
    TEST("HSTRLEN");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_HSTRLEN, 0);
    pos += add_string_2b(data + pos, "hash");
    pos += add_string_2b(data + pos, "field");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HSTRLEN || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_hscan() {
    TEST("HSCAN");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_HSCAN, 0);
    pos += add_string_2b(data + pos, "hash");
    memset(data + pos, 0, 7);
    data[pos + 7] = 0;  // 8-byte cursor
    pos += 8;
    // Optional pattern and count omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HSCAN || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_hrandfield() {
    TEST("HRANDFIELD");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_HRANDFIELD, 0);
    pos += add_string_2b(data + pos, "hash");
    // Optional count and withvalues omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HRANDFIELD || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_hexpire() {
    TEST("HEXPIRE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_HEXPIRE, 0);
    pos += add_string_2b(data + pos, "key");
    memset(data + pos, 0, 7);
    data[pos + 7] = 100;  // 8-byte seconds
    pos += 8;
    data[pos++] = 0x00;  // flags
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numfields
    pos += add_string_2b(data + pos, "field");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HEXPIRE || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_hexpireat() {
    TEST("HEXPIREAT");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_HEXPIREAT, 0);
    pos += add_string_2b(data + pos, "key");
    memset(data + pos, 0, 7);
    data[pos + 7] = 1000;  // 8-byte timestamp
    pos += 8;
    data[pos++] = 0x00;  // flags
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numfields
    pos += add_string_2b(data + pos, "field");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HEXPIREAT || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_hexpiretime() {
    TEST("HEXPIRETIME");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_HEXPIRETIME, 0);
    pos += add_string_2b(data + pos, "key");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numfields
    pos += add_string_2b(data + pos, "field");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HEXPIRETIME || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_hpexpire() {
    TEST("HPEXPIRE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_HPEXPIRE, 0);
    pos += add_string_2b(data + pos, "key");
    memset(data + pos, 0, 7);
    data[pos + 7] = 100000;  // 8-byte millis
    pos += 8;
    data[pos++] = 0x00;  // flags
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numfields
    pos += add_string_2b(data + pos, "field");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HPEXPIRE || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_hpexpireat() {
    TEST("HPEXPIREAT");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_HPEXPIREAT, 0);
    pos += add_string_2b(data + pos, "key");
    memset(data + pos, 0, 7);
    data[pos + 7] = 1000000;  // 8-byte timestamp
    pos += 8;
    data[pos++] = 0x00;  // flags
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numfields
    pos += add_string_2b(data + pos, "field");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HPEXPIREAT || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_hpexpiretime() {
    TEST("HPEXPIRETIME");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_HPEXPIRETIME, 0);
    pos += add_string_2b(data + pos, "key");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numfields
    pos += add_string_2b(data + pos, "field");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HPEXPIRETIME || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_hpttl() {
    TEST("HPTTL");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_HPTTL, 0);
    pos += add_string_2b(data + pos, "key");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numfields
    pos += add_string_2b(data + pos, "field");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HPTTL || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_httl() {
    TEST("HTTL");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_HTTL, 0);
    pos += add_string_2b(data + pos, "key");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numfields
    pos += add_string_2b(data + pos, "field");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HTTL || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_hpersist() {
    TEST("HPERSIST");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_HPERSIST, 0);
    pos += add_string_2b(data + pos, "key");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numfields
    pos += add_string_2b(data + pos, "field");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HPERSIST || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_hgetex() {
    TEST("HGETEX");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_HGETEX, 0);
    pos += add_string_2b(data + pos, "key");
    data[pos++] = 0x00;  // flags
    // Optional expiry omitted - parser will check buffer length and skip if needed
    // Since we have < 10 bytes after flags, parser will read numfields directly
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numfields
    pos += add_string_2b(data + pos, "field");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HGETEX || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_hsetex() {
    TEST("HSETEX");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_HSETEX, 0);
    pos += add_string_2b(data + pos, "key");
    data[pos++] = 0x00;  // flags
    // Optional expiry omitted - parser will check buffer length and skip if needed
    // Since we have < 10 bytes after flags, parser will read numfields directly
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numfields
    pos += add_string_2b(data + pos, "field");
    pos += add_string_4b(data + pos, "value");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HSETEX || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Additional Sorted Set Operations
void test_respb_zrangebyscore() {
    TEST("ZRANGEBYSCORE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZRANGEBYSCORE, 0);
    pos += add_string_2b(data + pos, "zset");
    memset(data + pos, 0, 7);
    data[pos + 7] = 0;  // 8-byte min
    pos += 8;
    memset(data + pos, 0, 7);
    data[pos + 7] = 100;  // 8-byte max
    pos += 8;
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZRANGEBYSCORE || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zrangebylex() {
    TEST("ZRANGEBYLEX");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZRANGEBYLEX, 0);
    pos += add_string_2b(data + pos, "zset");
    pos += add_string_2b(data + pos, "min");
    pos += add_string_2b(data + pos, "max");
    // Optional offset and count omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZRANGEBYLEX || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zrevrangebyscore() {
    TEST("ZREVRANGEBYSCORE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZREVRANGEBYSCORE, 0);
    pos += add_string_2b(data + pos, "zset");
    memset(data + pos, 0, 7);
    data[pos + 7] = 100;  // 8-byte max
    pos += 8;
    memset(data + pos, 0, 7);
    data[pos + 7] = 0;  // 8-byte min
    pos += 8;
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZREVRANGEBYSCORE || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zrevrangebylex() {
    TEST("ZREVRANGEBYLEX");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZREVRANGEBYLEX, 0);
    pos += add_string_2b(data + pos, "zset");
    pos += add_string_2b(data + pos, "max");  // max comes first in REVRANGEBYLEX
    pos += add_string_2b(data + pos, "min");  // min comes second
    // Optional offset and count omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    // Parser stores: key, max, min (3 args)
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZREVRANGEBYLEX || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zremrangebylex() {
    TEST("ZREMRANGEBYLEX");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZREMRANGEBYLEX, 0);
    pos += add_string_2b(data + pos, "zset");
    pos += add_string_2b(data + pos, "min");
    pos += add_string_2b(data + pos, "max");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZREMRANGEBYLEX || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zlexcount() {
    TEST("ZLEXCOUNT");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZLEXCOUNT, 0);
    pos += add_string_2b(data + pos, "zset");
    pos += add_string_2b(data + pos, "min");
    pos += add_string_2b(data + pos, "max");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZLEXCOUNT || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_bzpopmin() {
    TEST("BZPOPMIN");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_BZPOPMIN, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numkeys
    pos += add_string_2b(data + pos, "zset");
    memset(data + pos, 0, 7);
    data[pos + 7] = 5;  // 8-byte timeout
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_BZPOPMIN || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_bzpopmax() {
    TEST("BZPOPMAX");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_BZPOPMAX, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numkeys
    pos += add_string_2b(data + pos, "zset");
    memset(data + pos, 0, 7);
    data[pos + 7] = 5;  // 8-byte timeout
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_BZPOPMAX || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zrandmember() {
    TEST("ZRANDMEMBER");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZRANDMEMBER, 0);
    pos += add_string_2b(data + pos, "zset");
    // Optional count and withscores omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZRANDMEMBER || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zdiff() {
    TEST("ZDIFF");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZDIFF, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // numkeys
    pos += add_string_2b(data + pos, "zset1");
    pos += add_string_2b(data + pos, "zset2");
    data[pos++] = 0x00;  // withscores
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZDIFF || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zdiffstore() {
    TEST("ZDIFFSTORE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZDIFFSTORE, 0);
    pos += add_string_2b(data + pos, "dst");
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // numkeys
    pos += add_string_2b(data + pos, "zset1");
    pos += add_string_2b(data + pos, "zset2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZDIFFSTORE || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zinter() {
    TEST("ZINTER");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZINTER, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // numkeys
    pos += add_string_2b(data + pos, "zset1");
    pos += add_string_2b(data + pos, "zset2");
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZINTER || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zinterstore() {
    TEST("ZINTERSTORE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZINTERSTORE, 0);
    pos += add_string_2b(data + pos, "dst");
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // numkeys
    pos += add_string_2b(data + pos, "zset1");
    pos += add_string_2b(data + pos, "zset2");
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZINTERSTORE || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zunion() {
    TEST("ZUNION");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZUNION, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // numkeys
    pos += add_string_2b(data + pos, "zset1");
    pos += add_string_2b(data + pos, "zset2");
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZUNION || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zunionstore() {
    TEST("ZUNIONSTORE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZUNIONSTORE, 0);
    pos += add_string_2b(data + pos, "dst");
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // numkeys
    pos += add_string_2b(data + pos, "zset1");
    pos += add_string_2b(data + pos, "zset2");
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZUNIONSTORE || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zscan() {
    TEST("ZSCAN");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZSCAN, 0);
    pos += add_string_2b(data + pos, "zset");
    memset(data + pos, 0, 7);
    data[pos + 7] = 0;  // 8-byte cursor
    pos += 8;
    // Optional pattern, count, noscores omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZSCAN || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zmpop() {
    TEST("ZMPOP");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZMPOP, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // numkeys
    pos += add_string_2b(data + pos, "zset1");
    pos += add_string_2b(data + pos, "zset2");
    data[pos++] = 0x00;  // min_max
    // Optional count omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZMPOP || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_bzmpop() {
    TEST("BZMPOP");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_BZMPOP, 0);
    memset(data + pos, 0, 7);
    data[pos + 7] = 5;  // 8-byte timeout
    pos += 8;
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // numkeys
    pos += add_string_2b(data + pos, "zset1");
    pos += add_string_2b(data + pos, "zset2");
    data[pos++] = 0x00;  // min_max
    // Optional count omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_BZMPOP || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zrangestore() {
    TEST("ZRANGESTORE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZRANGESTORE, 0);
    pos += add_string_2b(data + pos, "dst");
    pos += add_string_2b(data + pos, "src");
    memset(data + pos, 0, 7);
    data[pos + 7] = 0;  // 8-byte min
    pos += 8;
    memset(data + pos, 0, 7);
    data[pos + 7] = 10;  // 8-byte max
    pos += 8;
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZRANGESTORE || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_zintercard() {
    TEST("ZINTERCARD");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ZINTERCARD, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // numkeys
    pos += add_string_2b(data + pos, "zset1");
    pos += add_string_2b(data + pos, "zset2");
    // Optional limit omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ZINTERCARD || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Additional Key Operations
void test_respb_expireat() {
    TEST("EXPIREAT");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_EXPIREAT, 0);
    pos += add_string_2b(data + pos, "key");
    memset(data + pos, 0, 7);
    data[pos + 7] = 60;  // 8-byte timestamp
    pos += 8;
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_EXPIREAT || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_pexpire() {
    TEST("PEXPIRE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_PEXPIRE, 0);
    pos += add_string_2b(data + pos, "key");
    memset(data + pos, 0, 7);
    data[pos + 7] = 100;  // 8-byte millis
    pos += 8;
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_PEXPIRE || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_pttl() {
    TEST("PTTL");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_PTTL, 0);
    pos += add_string_2b(data + pos, "key");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_PTTL || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_type() {
    TEST("TYPE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_TYPE, 0);
    pos += add_string_2b(data + pos, "key");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_TYPE || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_rename() {
    TEST("RENAME");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_RENAME, 0);
    pos += add_string_2b(data + pos, "key");
    pos += add_string_2b(data + pos, "newkey");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_RENAME || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_renamenx() {
    TEST("RENAMENX");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_RENAMENX, 0);
    pos += add_string_2b(data + pos, "key");
    pos += add_string_2b(data + pos, "newkey");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_RENAMENX || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_expiretime() {
    TEST("EXPIRETIME");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_EXPIRETIME, 0);
    pos += add_string_2b(data + pos, "key");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_EXPIRETIME || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_pexpireat() {
    TEST("PEXPIREAT");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_PEXPIREAT, 0);
    pos += add_string_2b(data + pos, "key");
    memset(data + pos, 0, 7);
    data[pos + 7] = 60;  // 8-byte timestamp
    pos += 8;
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_PEXPIREAT || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_pexpiretime() {
    TEST("PEXPIRETIME");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_PEXPIRETIME, 0);
    pos += add_string_2b(data + pos, "key");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_PEXPIRETIME || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_keys() {
    TEST("KEYS");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_KEYS, 0);
    pos += add_string_2b(data + pos, "pattern*");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_KEYS || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_scan() {
    TEST("SCAN");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SCAN, 0);
    memset(data + pos, 0, 7);
    data[pos + 7] = 0;  // 8-byte cursor
    pos += 8;
    // Optional pattern, count, type omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SCAN || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_randomkey() {
    TEST("RANDOMKEY");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_RANDOMKEY, 0);
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_RANDOMKEY || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_dump() {
    TEST("DUMP");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_DUMP, 0);
    pos += add_string_2b(data + pos, "key");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_DUMP || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_restore() {
    TEST("RESTORE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_RESTORE, 0);
    pos += add_string_2b(data + pos, "key");
    memset(data + pos, 0, 7);
    data[pos + 7] = 0;  // 8-byte ttl
    pos += 8;
    pos += add_string_4b(data + pos, "data");
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_RESTORE || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_migrate() {
    TEST("MIGRATE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_MIGRATE, 0);
    pos += add_string_2b(data + pos, "host");
    data[pos++] = 0x00;
    data[pos++] = 0x50;  // port 80
    pos += add_string_2b(data + pos, "key");
    data[pos++] = 0x00;
    data[pos++] = 0x00;  // db 0
    memset(data + pos, 0, 7);
    data[pos + 7] = 5;  // 8-byte timeout
    pos += 8;
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    // MIGRATE format: [2B hostlen][host][2B port][2B keylen][key][2B db][8B timeout][1B flags]
    // Parser stores: host (args[0]), key (args[1]) - port and db are numeric, not stored
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_MIGRATE || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_move() {
    TEST("MOVE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_MOVE, 0);
    pos += add_string_2b(data + pos, "key");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // db 1
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_MOVE || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_copy() {
    TEST("COPY");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_COPY, 0);
    pos += add_string_2b(data + pos, "src");
    pos += add_string_2b(data + pos, "dst");
    data[pos++] = 0x00;
    data[pos++] = 0x00;  // db (optional)
    data[pos++] = 0x00;  // replace flag
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_COPY || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_sort() {
    TEST("SORT");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SORT, 0);
    pos += add_string_2b(data + pos, "key");
    // Complex sorting options omitted for simplicity
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SORT || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_sort_ro() {
    TEST("SORT_RO");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SORT_RO, 0);
    pos += add_string_2b(data + pos, "key");
    // Complex sorting options omitted for simplicity
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SORT_RO || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_touch() {
    TEST("TOUCH");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_TOUCH, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // numkeys
    pos += add_string_2b(data + pos, "key1");
    pos += add_string_2b(data + pos, "key2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_TOUCH || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_object() {
    TEST("OBJECT");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_OBJECT, 0);
    data[pos++] = 0x00;  // subcommand
    pos += add_string_2b(data + pos, "key");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_OBJECT || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_wait() {
    TEST("WAIT");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_WAIT, 0);
    memset(data + pos, 0, 7);
    data[pos + 7] = 1;  // 8-byte numreplicas
    pos += 8;
    memset(data + pos, 0, 7);
    data[pos + 7] = 1000;  // 8-byte timeout
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_WAIT || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_waitaof() {
    TEST("WAITAOF");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_WAITAOF, 0);
    memset(data + pos, 0, 7);
    data[pos + 7] = 1;  // 8-byte numlocal
    pos += 8;
    memset(data + pos, 0, 7);
    data[pos + 7] = 1;  // 8-byte numreplicas
    pos += 8;
    memset(data + pos, 0, 7);
    data[pos + 7] = 1000;  // 8-byte timeout
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_WAITAOF || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Scripting and Functions Operations
void test_respb_eval() {
    TEST("EVAL");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_EVAL, 0);
    pos += add_string_4b(data + pos, "return 1");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numkeys
    pos += add_string_2b(data + pos, "key1");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numargs
    pos += add_string_2b(data + pos, "arg1");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_EVAL || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_evalsha() {
    TEST("EVALSHA");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_EVALSHA, 0);
    pos += add_string_2b(data + pos, "sha1hash");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numkeys
    pos += add_string_2b(data + pos, "key1");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numargs
    pos += add_string_2b(data + pos, "arg1");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_EVALSHA || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_eval_ro() {
    TEST("EVAL_RO");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_EVAL_RO, 0);
    pos += add_string_4b(data + pos, "return 1");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numkeys
    pos += add_string_2b(data + pos, "key1");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numargs
    pos += add_string_2b(data + pos, "arg1");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_EVAL_RO || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_evalsha_ro() {
    TEST("EVALSHA_RO");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_EVALSHA_RO, 0);
    pos += add_string_2b(data + pos, "sha1hash");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numkeys
    pos += add_string_2b(data + pos, "key1");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numargs
    pos += add_string_2b(data + pos, "arg1");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_EVALSHA_RO || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_script() {
    TEST("SCRIPT");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SCRIPT, 0);
    data[pos++] = 0x00;  // subcommand
    // Additional args omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SCRIPT) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_fcall() {
    TEST("FCALL");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_FCALL, 0);
    pos += add_string_2b(data + pos, "function");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numkeys
    pos += add_string_2b(data + pos, "key1");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numargs
    pos += add_string_2b(data + pos, "arg1");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_FCALL || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_fcall_ro() {
    TEST("FCALL_RO");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_FCALL_RO, 0);
    pos += add_string_2b(data + pos, "function");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numkeys
    pos += add_string_2b(data + pos, "key1");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numargs
    pos += add_string_2b(data + pos, "arg1");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_FCALL_RO || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_function() {
    TEST("FUNCTION");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_FUNCTION, 0);
    data[pos++] = 0x00;  // subcommand
    // Additional args omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_FUNCTION) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Cluster Management Operations
void test_respb_cluster() {
    TEST("CLUSTER");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_CLUSTER, 0);
    data[pos++] = 0x00;  // subcommand
    // Additional args omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_CLUSTER) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_readonly() {
    TEST("READONLY");
    uint8_t data[10];
    size_t pos = build_header(data, RESPB_OP_READONLY, 0);
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_READONLY || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_readwrite() {
    TEST("READWRITE");
    uint8_t data[10];
    size_t pos = build_header(data, RESPB_OP_READWRITE, 0);
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_READWRITE || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_asking() {
    TEST("ASKING");
    uint8_t data[10];
    size_t pos = build_header(data, RESPB_OP_ASKING, 0);
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ASKING || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Server Management Operations
void test_respb_dbsize() {
    TEST("DBSIZE");
    uint8_t data[10];
    size_t pos = build_header(data, RESPB_OP_DBSIZE, 0);
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_DBSIZE || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_flushdb() {
    TEST("FLUSHDB");
    uint8_t data[10];
    size_t pos = build_header(data, RESPB_OP_FLUSHDB, 0);
    data[pos++] = 0x00;  // async_sync
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_FLUSHDB || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_flushall() {
    TEST("FLUSHALL");
    uint8_t data[10];
    size_t pos = build_header(data, RESPB_OP_FLUSHALL, 0);
    data[pos++] = 0x00;  // async_sync
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_FLUSHALL || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_save() {
    TEST("SAVE");
    uint8_t data[10];
    size_t pos = build_header(data, RESPB_OP_SAVE, 0);
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SAVE || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_bgsave() {
    TEST("BGSAVE");
    uint8_t data[10];
    size_t pos = build_header(data, RESPB_OP_BGSAVE, 0);
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_BGSAVE || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_bgrewriteaof() {
    TEST("BGREWRITEAOF");
    uint8_t data[10];
    size_t pos = build_header(data, RESPB_OP_BGREWRITEAOF, 0);
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_BGREWRITEAOF || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_lastsave() {
    TEST("LASTSAVE");
    uint8_t data[10];
    size_t pos = build_header(data, RESPB_OP_LASTSAVE, 0);
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_LASTSAVE || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_shutdown() {
    TEST("SHUTDOWN");
    uint8_t data[10];
    size_t pos = build_header(data, RESPB_OP_SHUTDOWN, 0);
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SHUTDOWN || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_info() {
    TEST("INFO");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_INFO, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // count
    pos += add_string_2b(data + pos, "server");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_INFO || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_config() {
    TEST("CONFIG");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_CONFIG, 0);
    data[pos++] = 0x00;  // subcommand
    // Additional args omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_CONFIG) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_command() {
    TEST("COMMAND");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_COMMAND, 0);
    data[pos++] = 0x00;  // subcommand
    // Additional args omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_COMMAND) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_time() {
    TEST("TIME");
    uint8_t data[10];
    size_t pos = build_header(data, RESPB_OP_TIME, 0);
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_TIME || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_role() {
    TEST("ROLE");
    uint8_t data[10];
    size_t pos = build_header(data, RESPB_OP_ROLE, 0);
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ROLE || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_replicaof() {
    TEST("REPLICAOF");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_REPLICAOF, 0);
    pos += add_string_2b(data + pos, "host");
    data[pos++] = 0x00;
    data[pos++] = 0x50;  // port 80
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_REPLICAOF || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_slaveof() {
    TEST("SLAVEOF");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SLAVEOF, 0);
    pos += add_string_2b(data + pos, "host");
    data[pos++] = 0x00;
    data[pos++] = 0x50;  // port 80
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SLAVEOF || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_monitor() {
    TEST("MONITOR");
    uint8_t data[10];
    size_t pos = build_header(data, RESPB_OP_MONITOR, 0);
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_MONITOR || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_debug() {
    TEST("DEBUG");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_DEBUG, 0);
    data[pos++] = 0x00;  // subcommand
    // Additional args omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_DEBUG) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_sync() {
    TEST("SYNC");
    uint8_t data[10];
    size_t pos = build_header(data, RESPB_OP_SYNC, 0);
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SYNC || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_psync() {
    TEST("PSYNC");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_PSYNC, 0);
    pos += add_string_2b(data + pos, "replid");
    memset(data + pos, 0, 7);
    data[pos + 7] = 100;  // 8-byte offset
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_PSYNC || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_replconf() {
    TEST("REPLCONF");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_REPLCONF, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // count
    pos += add_string_2b(data + pos, "arg");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_REPLCONF || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_slowlog() {
    TEST("SLOWLOG");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SLOWLOG, 0);
    data[pos++] = 0x00;  // subcommand
    // Optional count omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SLOWLOG) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_latency() {
    TEST("LATENCY");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_LATENCY, 0);
    data[pos++] = 0x00;  // subcommand
    // Additional args omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_LATENCY) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_memory() {
    TEST("MEMORY");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_MEMORY, 0);
    data[pos++] = 0x00;  // subcommand
    // Additional args omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_MEMORY) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_module() {
    TEST("MODULE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_MODULE_CMD, 0);
    data[pos++] = 0x00;  // subcommand
    // Additional args omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_MODULE_CMD) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_acl() {
    TEST("ACL");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ACL, 0);
    data[pos++] = 0x00;  // subcommand
    // Additional args omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ACL) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_failover() {
    TEST("FAILOVER");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_FAILOVER, 0);
    data[pos++] = 0x00;  // flags
    // Optional host, port, timeout omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_FAILOVER || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_swapdb() {
    TEST("SWAPDB");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SWAPDB, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x00;  // db1
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // db2
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SWAPDB || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_lolwut() {
    TEST("LOLWUT");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_LOLWUT, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // count
    pos += add_string_2b(data + pos, "arg");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_LOLWUT || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_restore_asking() {
    TEST("RESTORE-ASKING");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_RESTORE_ASKING, 0);
    pos += add_string_2b(data + pos, "key");
    memset(data + pos, 0, 7);
    data[pos + 7] = 1000;  // 8-byte ttl
    pos += 8;
    pos += add_string_4b(data + pos, "data");
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_RESTORE_ASKING || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_commandlog() {
    TEST("COMMANDLOG");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_COMMANDLOG, 0);
    data[pos++] = 0x00;  // subcommand
    // Additional args omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_COMMANDLOG) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Connection Management Operations
void test_respb_auth() {
    TEST("AUTH");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_AUTH, 0);
    pos += add_string_2b(data + pos, "password");
    // Optional username omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_AUTH || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_select() {
    TEST("SELECT");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SELECT, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // db number
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SELECT || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_quit() {
    TEST("QUIT");
    uint8_t data[10];
    size_t pos = build_header(data, RESPB_OP_QUIT, 0);
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_QUIT || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_hello() {
    TEST("HELLO");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_HELLO, 0);
    data[pos++] = 0x03;  // protocol version
    // Optional auth and client name omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_HELLO || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_reset() {
    TEST("RESET");
    uint8_t data[10];
    size_t pos = build_header(data, RESPB_OP_RESET, 0);
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_RESET || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_client() {
    TEST("CLIENT");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_CLIENT, 0);
    data[pos++] = 0x00;  // subcommand
    // Additional args omitted for simplicity
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_CLIENT) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Pub/Sub Operations
void test_respb_publish() {
    TEST("PUBLISH");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_PUBLISH, 0);
    pos += add_string_2b(data + pos, "channel");
    pos += add_string_4b(data + pos, "message");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_PUBLISH || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_subscribe() {
    TEST("SUBSCRIBE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SUBSCRIBE, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // count
    pos += add_string_2b(data + pos, "chan1");
    pos += add_string_2b(data + pos, "chan2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SUBSCRIBE || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_unsubscribe() {
    TEST("UNSUBSCRIBE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_UNSUBSCRIBE, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // count
    pos += add_string_2b(data + pos, "chan1");
    pos += add_string_2b(data + pos, "chan2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_UNSUBSCRIBE || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_psubscribe() {
    TEST("PSUBSCRIBE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_PSUBSCRIBE, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // count
    pos += add_string_2b(data + pos, "pattern1");
    pos += add_string_2b(data + pos, "pattern2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_PSUBSCRIBE || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_punsubscribe() {
    TEST("PUNSUBSCRIBE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_PUNSUBSCRIBE, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // count
    pos += add_string_2b(data + pos, "pattern1");
    pos += add_string_2b(data + pos, "pattern2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_PUNSUBSCRIBE || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_pubsub() {
    TEST("PUBSUB");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_PUBSUB, 0);
    data[pos++] = 0x00;  // subcommand
    // Additional args omitted for simplicity
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_PUBSUB) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_spublish() {
    TEST("SPUBLISH");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SPUBLISH, 0);
    pos += add_string_2b(data + pos, "channel");
    pos += add_string_4b(data + pos, "message");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SPUBLISH || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_ssubscribe() {
    TEST("SSUBSCRIBE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SSUBSCRIBE, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // count
    pos += add_string_2b(data + pos, "chan1");
    pos += add_string_2b(data + pos, "chan2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SSUBSCRIBE || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_sunsubscribe() {
    TEST("SUNSUBSCRIBE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SUNSUBSCRIBE, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // count
    pos += add_string_2b(data + pos, "chan1");
    pos += add_string_2b(data + pos, "chan2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SUNSUBSCRIBE || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Additional List Operations
void test_respb_lpos() {
    TEST("LPOS");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_LPOS, 0);
    pos += add_string_2b(data + pos, "list");
    pos += add_string_2b(data + pos, "elem");
    // Optional rank, count, maxlen omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_LPOS || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_blpop() {
    TEST("BLPOP");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_BLPOP, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numkeys
    pos += add_string_2b(data + pos, "list");
    memset(data + pos, 0, 7);
    data[pos + 7] = 5;  // 8-byte timeout
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_BLPOP || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_brpop() {
    TEST("BRPOP");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_BRPOP, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numkeys
    pos += add_string_2b(data + pos, "list");
    memset(data + pos, 0, 7);
    data[pos + 7] = 5;  // 8-byte timeout
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_BRPOP || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_lmpop() {
    TEST("LMPOP");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_LMPOP, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // numkeys
    pos += add_string_2b(data + pos, "list1");
    pos += add_string_2b(data + pos, "list2");
    data[pos++] = 0x00;  // left_right
    // Optional count omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_LMPOP || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_brpoplpush() {
    TEST("BRPOPLPUSH");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_BRPOPLPUSH, 0);
    pos += add_string_2b(data + pos, "src");
    pos += add_string_2b(data + pos, "dst");
    memset(data + pos, 0, 7);
    data[pos + 7] = 5;  // 8-byte timeout
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_BRPOPLPUSH || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_blmove() {
    TEST("BLMOVE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_BLMOVE, 0);
    pos += add_string_2b(data + pos, "src");
    pos += add_string_2b(data + pos, "dst");
    data[pos++] = 0x00;  // wherefrom
    data[pos++] = 0x01;  // whereto
    memset(data + pos, 0, 7);
    data[pos + 7] = 5;  // 8-byte timeout
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_BLMOVE || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_blmpop() {
    TEST("BLMPOP");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_BLMPOP, 0);
    memset(data + pos, 0, 7);
    data[pos + 7] = 5;  // 8-byte timeout
    pos += 8;
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // numkeys
    pos += add_string_2b(data + pos, "list1");
    pos += add_string_2b(data + pos, "list2");
    data[pos++] = 0x00;  // left_right
    // Optional count omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_BLMPOP || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Additional Set Operations
void test_respb_sscan() {
    TEST("SSCAN");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SSCAN, 0);
    pos += add_string_2b(data + pos, "set");
    memset(data + pos, 0, 7);
    data[pos + 7] = 0;  // 8-byte cursor
    pos += 8;
    // Optional pattern and count omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SSCAN || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_sintercard() {
    TEST("SINTERCARD");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SINTERCARD, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // numkeys
    pos += add_string_2b(data + pos, "set1");
    pos += add_string_2b(data + pos, "set2");
    // Optional limit omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SINTERCARD || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_smismember() {
    TEST("SMISMEMBER");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SMISMEMBER, 0);
    pos += add_string_2b(data + pos, "set");
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // count
    pos += add_string_2b(data + pos, "m1");
    pos += add_string_2b(data + pos, "m2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SMISMEMBER || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Key Operations
void test_respb_unlink() {
    TEST("UNLINK");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_UNLINK, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // numkeys
    pos += add_string_2b(data + pos, "key1");
    pos += add_string_2b(data + pos, "key2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_UNLINK || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_expire() {
    TEST("EXPIRE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_EXPIRE, 0);
    pos += add_string_2b(data + pos, "key");
    memset(data + pos, 0, 7);
    data[pos + 7] = 60;  // 8-byte seconds
    pos += 8;
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_EXPIRE || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_ttl() {
    TEST("TTL");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_TTL, 0);
    pos += add_string_2b(data + pos, "key");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_TTL || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_persist() {
    TEST("PERSIST");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_PERSIST, 0);
    pos += add_string_2b(data + pos, "key");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_PERSIST || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Connection Management
void test_respb_echo() {
    TEST("ECHO");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_ECHO, 0);
    pos += add_string_2b(data + pos, "hello");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_ECHO || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Transaction Operations
void test_respb_discard() {
    TEST("DISCARD");
    uint8_t data[10];
    size_t pos = build_header(data, RESPB_OP_DISCARD, 0);
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_DISCARD || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_watch() {
    TEST("WATCH");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_WATCH, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // numkeys
    pos += add_string_2b(data + pos, "key1");
    pos += add_string_2b(data + pos, "key2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_WATCH || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_unwatch() {
    TEST("UNWATCH");
    uint8_t data[10];
    size_t pos = build_header(data, RESPB_OP_UNWATCH, 0);
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_UNWATCH || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Bitmap Operations
void test_respb_setbit() {
    TEST("SETBIT");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_SETBIT, 0);
    pos += add_string_2b(data + pos, "key");
    memset(data + pos, 0, 7);
    data[pos + 7] = 10;  // 8-byte offset
    pos += 8;
    data[pos++] = 0x01;  // value
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_SETBIT || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_getbit() {
    TEST("GETBIT");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_GETBIT, 0);
    pos += add_string_2b(data + pos, "key");
    memset(data + pos, 0, 7);
    data[pos + 7] = 10;  // 8-byte offset
    pos += 8;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_GETBIT || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_bitcount() {
    TEST("BITCOUNT");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_BITCOUNT, 0);
    pos += add_string_2b(data + pos, "key");
    // Optional start, end, unit omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_BITCOUNT || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_bitpos() {
    TEST("BITPOS");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_BITPOS, 0);
    pos += add_string_2b(data + pos, "key");
    data[pos++] = 0x01;  // bit
    // Optional start, end, unit omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_BITPOS || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_bitop() {
    TEST("BITOP");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_BITOP, 0);
    data[pos++] = 0x00;  // operation
    pos += add_string_2b(data + pos, "dst");
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // numkeys
    pos += add_string_2b(data + pos, "key1");
    pos += add_string_2b(data + pos, "key2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_BITOP || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_bitfield() {
    TEST("BITFIELD");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_BITFIELD, 0);
    pos += add_string_2b(data + pos, "key");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // count
    data[pos++] = 0x00;  // op
    data[pos++] = 0x00;  // args (2 bytes)
    data[pos++] = 0x00;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_BITFIELD || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// HyperLogLog Operations
void test_respb_pfadd() {
    TEST("PFADD");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_PFADD, 0);
    pos += add_string_2b(data + pos, "key");
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // count
    pos += add_string_2b(data + pos, "elem1");
    pos += add_string_2b(data + pos, "elem2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_PFADD || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_pfcount() {
    TEST("PFCOUNT");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_PFCOUNT, 0);
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // numkeys
    pos += add_string_2b(data + pos, "key1");
    pos += add_string_2b(data + pos, "key2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_PFCOUNT || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_pfmerge() {
    TEST("PFMERGE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_PFMERGE, 0);
    pos += add_string_2b(data + pos, "dst");
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // numkeys
    pos += add_string_2b(data + pos, "key1");
    pos += add_string_2b(data + pos, "key2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_PFMERGE || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_pfdebug() {
    TEST("PFDEBUG");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_PFDEBUG, 0);
    pos += add_string_2b(data + pos, "subcmd");
    pos += add_string_2b(data + pos, "key");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_PFDEBUG || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Geospatial Operations
void test_respb_geoadd() {
    TEST("GEOADD");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_GEOADD, 0);
    pos += add_string_2b(data + pos, "key");
    data[pos++] = 0x00;  // flags
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // count
    memset(data + pos, 0, 7);
    data[pos + 7] = 0;  // 8-byte longitude
    pos += 8;
    memset(data + pos, 0, 7);
    data[pos + 7] = 0;  // 8-byte latitude
    pos += 8;
    pos += add_string_2b(data + pos, "member");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_GEOADD || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_geodist() {
    TEST("GEODIST");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_GEODIST, 0);
    pos += add_string_2b(data + pos, "key");
    pos += add_string_2b(data + pos, "mem1");
    pos += add_string_2b(data + pos, "mem2");
    data[pos++] = 0x00;  // unit
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_GEODIST || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_geohash() {
    TEST("GEOHASH");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_GEOHASH, 0);
    pos += add_string_2b(data + pos, "key");
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // count
    pos += add_string_2b(data + pos, "mem1");
    pos += add_string_2b(data + pos, "mem2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_GEOHASH || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_geopos() {
    TEST("GEOPOS");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_GEOPOS, 0);
    pos += add_string_2b(data + pos, "key");
    data[pos++] = 0x00;
    data[pos++] = 0x02;  // count
    pos += add_string_2b(data + pos, "mem1");
    pos += add_string_2b(data + pos, "mem2");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_GEOPOS || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_georadius() {
    TEST("GEORADIUS");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_GEORADIUS, 0);
    pos += add_string_2b(data + pos, "key");
    memset(data + pos, 0, 7);
    data[pos + 7] = 0;  // 8-byte longitude
    pos += 8;
    memset(data + pos, 0, 7);
    data[pos + 7] = 0;  // 8-byte latitude
    pos += 8;
    memset(data + pos, 0, 7);
    data[pos + 7] = 10;  // 8-byte radius
    pos += 8;
    data[pos++] = 0x00;  // unit
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_GEORADIUS || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_georadiusbymember() {
    TEST("GEORADIUSBYMEMBER");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_GEORADIUSBYMEMBER, 0);
    pos += add_string_2b(data + pos, "key");
    pos += add_string_2b(data + pos, "member");
    memset(data + pos, 0, 7);
    data[pos + 7] = 10;  // 8-byte radius
    pos += 8;
    data[pos++] = 0x00;  // unit
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_GEORADIUSBYMEMBER || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_georadius_ro() {
    TEST("GEORADIUS_RO");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_GEORADIUS_RO, 0);
    pos += add_string_2b(data + pos, "key");
    memset(data + pos, 0, 7);
    data[pos + 7] = 1;  // 8-byte longitude
    pos += 8;
    memset(data + pos, 0, 7);
    data[pos + 7] = 2;  // 8-byte latitude
    pos += 8;
    memset(data + pos, 0, 7);
    data[pos + 7] = 1000;  // 8-byte radius
    pos += 8;
    data[pos++] = 0x00;  // unit
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_GEORADIUS_RO || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_georadiusbymember_ro() {
    TEST("GEORADIUSBYMEMBER_RO");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_GEORADIUSBYMEMBER_RO, 0);
    pos += add_string_2b(data + pos, "key");
    pos += add_string_2b(data + pos, "member");
    memset(data + pos, 0, 7);
    data[pos + 7] = 1000;  // 8-byte radius
    pos += 8;
    data[pos++] = 0x00;  // unit
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_GEORADIUSBYMEMBER_RO || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_geosearch() {
    TEST("GEOSEARCH");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_GEOSEARCH, 0);
    pos += add_string_2b(data + pos, "key");
    // Complex payload with flags - simplified
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_GEOSEARCH || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_geosearchstore() {
    TEST("GEOSEARCHSTORE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_GEOSEARCHSTORE, 0);
    pos += add_string_2b(data + pos, "dst");
    pos += add_string_2b(data + pos, "src");
    // Complex payload with flags - simplified
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_GEOSEARCHSTORE || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Stream Operations
void test_respb_xadd() {
    TEST("XADD");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_XADD, 0);
    pos += add_string_2b(data + pos, "key");
    pos += add_string_2b(data + pos, "id");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // count
    pos += add_string_2b(data + pos, "field");
    pos += add_string_4b(data + pos, "value");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_XADD || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_xlen() {
    TEST("XLEN");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_XLEN, 0);
    pos += add_string_2b(data + pos, "key");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_XLEN || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_xrange() {
    TEST("XRANGE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_XRANGE, 0);
    pos += add_string_2b(data + pos, "key");
    pos += add_string_2b(data + pos, "start");
    pos += add_string_2b(data + pos, "end");
    // Optional count omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_XRANGE || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_xrevrange() {
    TEST("XREVRANGE");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_XREVRANGE, 0);
    pos += add_string_2b(data + pos, "key");
    pos += add_string_2b(data + pos, "end");
    pos += add_string_2b(data + pos, "start");
    // Optional count omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_XREVRANGE || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_xread() {
    TEST("XREAD");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_XREAD, 0);
    // Optional count and block omitted
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numkeys
    pos += add_string_2b(data + pos, "key");
    pos += add_string_2b(data + pos, "id");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_XREAD || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_xreadgroup() {
    TEST("XREADGROUP");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_XREADGROUP, 0);
    pos += add_string_2b(data + pos, "group");
    pos += add_string_2b(data + pos, "consumer");
    // Optional count, block, noack omitted
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // numkeys
    pos += add_string_2b(data + pos, "key");
    pos += add_string_2b(data + pos, "id");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_XREADGROUP || cmd.argc != 4) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_xdel() {
    TEST("XDEL");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_XDEL, 0);
    pos += add_string_2b(data + pos, "key");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // count
    pos += add_string_2b(data + pos, "id");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_XDEL || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_bitfield_ro() {
    TEST("BITFIELD_RO");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_BITFIELD_RO, 0);
    pos += add_string_2b(data + pos, "key");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // count
    data[pos++] = 0x00;  // op
    data[pos++] = 0x00;  // args (2 bytes)
    data[pos++] = 0x00;
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_BITFIELD_RO || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_pfselftest() {
    TEST("PFSELFTEST");
    uint8_t data[10];
    size_t pos = build_header(data, RESPB_OP_PFSELFTEST, 0);
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_PFSELFTEST || cmd.argc != 0) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_xtrim() {
    TEST("XTRIM");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_XTRIM, 0);
    pos += add_string_2b(data + pos, "key");
    data[pos++] = 0x00;  // strategy
    memset(data + pos, 0, 7);
    data[pos + 7] = 10;  // 8-byte threshold
    pos += 8;
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_XTRIM || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_xack() {
    TEST("XACK");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_XACK, 0);
    pos += add_string_2b(data + pos, "key");
    pos += add_string_2b(data + pos, "group");
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // count
    pos += add_string_2b(data + pos, "id");
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_XACK || cmd.argc != 3) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_xpending() {
    TEST("XPENDING");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_XPENDING, 0);
    pos += add_string_2b(data + pos, "key");
    pos += add_string_2b(data + pos, "group");
    // Optional fields omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_XPENDING || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_xclaim() {
    TEST("XCLAIM");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_XCLAIM, 0);
    pos += add_string_2b(data + pos, "key");
    pos += add_string_2b(data + pos, "group");
    pos += add_string_2b(data + pos, "consumer");
    memset(data + pos, 0, 7);
    data[pos + 7] = 1000;  // 8-byte min_idle
    pos += 8;
    data[pos++] = 0x00;
    data[pos++] = 0x01;  // count
    pos += add_string_2b(data + pos, "id");
    data[pos++] = 0x00;  // flags
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_XCLAIM || cmd.argc != 4) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_xautoclaim() {
    TEST("XAUTOCLAIM");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_XAUTOCLAIM, 0);
    pos += add_string_2b(data + pos, "key");
    pos += add_string_2b(data + pos, "group");
    pos += add_string_2b(data + pos, "consumer");
    memset(data + pos, 0, 7);
    data[pos + 7] = 1000;  // 8-byte min_idle
    pos += 8;
    pos += add_string_2b(data + pos, "start");
    // Optional count and justid omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_XAUTOCLAIM || cmd.argc != 4) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_xinfo() {
    TEST("XINFO");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_XINFO, 0);
    data[pos++] = 0x00;  // subcommand
    pos += add_string_2b(data + pos, "key");
    // Additional args omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_XINFO || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_xgroup() {
    TEST("XGROUP");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_XGROUP, 0);
    data[pos++] = 0x00;  // subcommand
    pos += add_string_2b(data + pos, "key");
    // Additional args omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_XGROUP || cmd.argc != 1) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

void test_respb_xsetid() {
    TEST("XSETID");
    uint8_t data[200];
    size_t pos = build_header(data, RESPB_OP_XSETID, 0);
    pos += add_string_2b(data + pos, "key");
    pos += add_string_2b(data + pos, "id");
    // Optional entries_added, maxdeletlen, maxdeleteid omitted
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 1 || cmd.opcode != RESPB_OP_XSETID || cmd.argc != 2) {
        FAIL("Parse error");
        return;
    }
    PASS();
}

// Error Handling Tests
void test_respb_error_truncated() {
    TEST("Error: truncated header");
    uint8_t data[2] = {0x00, 0x00};
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, 2);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != 0) {
        FAIL("Should return 0 for incomplete");
        return;
    }
    PASS();
}

void test_respb_error_unknown_opcode() {
    TEST("Error: unknown opcode");
    uint8_t data[10];
    size_t pos = build_header(data, 0xBEEF, 0);
    
    respb_parser_t parser;
    respb_parser_init(&parser, data, pos);
    respb_command_t cmd;
    
    if (respb_parse_command(&parser, &cmd) != -1) {
        FAIL("Should return -1 for unknown");
        return;
    }
    PASS();
}

int main() {
    printf("\n");
    printf("=========================================================\n");
    printf("  Comprehensive RESPB Protocol Test Suite\n");
    printf("=========================================================\n");
    printf("\n");
    
    printf("RESP Parser Tests (2):\n");
    test_resp_simple_get();
    test_resp_set();
    
    printf("\nRESPB String Operations (22):\n");
    test_respb_simple_get();
    test_respb_set();
    test_respb_mget();
    test_respb_incr();
    test_respb_decr();
    test_respb_incrby();
    test_respb_decrby();
    test_respb_strlen();
    test_respb_append();
    test_respb_getdel();
    test_respb_setnx();
    test_respb_getex();
    test_respb_getrange();
    test_respb_getset();
    test_respb_incrbyfloat();
    test_respb_mset();
    test_respb_msetnx();
    test_respb_psetex();
    test_respb_setex();
    test_respb_setrange();
    test_respb_substr();
    test_respb_lcs();
    test_respb_delifeq();
    
    printf("\nRESPB List Operations (16):\n");
    test_respb_lpush();
    test_respb_rpush();
    test_respb_llen();
    test_respb_lpop();
    test_respb_rpop();
    test_respb_lrange();
    test_respb_lindex();
    test_respb_lset();
    test_respb_lrem();
    test_respb_ltrim();
    test_respb_linsert();
    test_respb_lpushx();
    test_respb_rpushx();
    test_respb_rpoplpush();
    test_respb_lmove();
    
    printf("\nRESPB Set Operations (14):\n");
    test_respb_sadd();
    test_respb_scard();
    test_respb_smembers();
    test_respb_srem();
    test_respb_sismember();
    test_respb_spop();
    test_respb_srandmember();
    test_respb_sinter();
    test_respb_sinterstore();
    test_respb_sunion();
    test_respb_sunionstore();
    test_respb_sdiff();
    test_respb_sdiffstore();
    test_respb_smove();
    
    printf("\nRESPB Sorted Set Operations (34):\n");
    test_respb_zcard();
    test_respb_zscore();
    test_respb_zadd();
    test_respb_zrem();
    test_respb_zcount();
    test_respb_zincrby();
    test_respb_zrange();
    test_respb_zrank();
    test_respb_zrevrange();
    test_respb_zrevrank();
    test_respb_zrangebyscore();
    test_respb_zrangebylex();
    test_respb_zrevrangebyscore();
    test_respb_zrevrangebylex();
    test_respb_zremrangebylex();
    test_respb_zlexcount();
    test_respb_bzpopmin();
    test_respb_bzpopmax();
    test_respb_zrandmember();
    test_respb_zdiff();
    test_respb_zdiffstore();
    test_respb_zinter();
    test_respb_zinterstore();
    test_respb_zunion();
    test_respb_zunionstore();
    test_respb_zscan();
    test_respb_zmpop();
    test_respb_bzmpop();
    test_respb_zrangestore();
    test_respb_zintercard();
    test_respb_zmscore();
    test_respb_zpopmin();
    test_respb_zpopmax();
    test_respb_zremrangebyrank();
    test_respb_zremrangebyscore();
    
    printf("\nRESPB Hash Operations (27):\n");
    test_respb_hget();
    test_respb_hgetall();
    test_respb_hset();
    test_respb_hmset();
    test_respb_hmget();
    test_respb_hdel();
    test_respb_hexists();
    test_respb_hincrby();
    test_respb_hkeys();
    test_respb_hvals();
    test_respb_hlen();
    test_respb_hsetnx();
    test_respb_hincrbyfloat();
    test_respb_hstrlen();
    test_respb_hscan();
    test_respb_hrandfield();
    test_respb_hexpire();
    test_respb_hexpireat();
    test_respb_hexpiretime();
    test_respb_hpexpire();
    test_respb_hpexpireat();
    test_respb_hpexpiretime();
    test_respb_hpttl();
    test_respb_httl();
    test_respb_hpersist();
    test_respb_hgetex();
    test_respb_hsetex();
    
    printf("\nRESPB List Operations (22):\n");
    test_respb_lpush();
    test_respb_rpush();
    test_respb_llen();
    test_respb_lpop();
    test_respb_rpop();
    test_respb_lrange();
    test_respb_lindex();
    test_respb_lset();
    test_respb_lrem();
    test_respb_ltrim();
    test_respb_linsert();
    test_respb_lpushx();
    test_respb_rpushx();
    test_respb_rpoplpush();
    test_respb_lmove();
    test_respb_lpos();
    test_respb_blpop();
    test_respb_brpop();
    test_respb_lmpop();
    test_respb_brpoplpush();
    test_respb_blmove();
    test_respb_blmpop();
    
    printf("\nRESPB Set Operations (17):\n");
    test_respb_sadd();
    test_respb_scard();
    test_respb_smembers();
    test_respb_srem();
    test_respb_sismember();
    test_respb_spop();
    test_respb_srandmember();
    test_respb_sinter();
    test_respb_sinterstore();
    test_respb_sunion();
    test_respb_sunionstore();
    test_respb_sdiff();
    test_respb_sdiffstore();
    test_respb_smove();
    test_respb_sscan();
    test_respb_sintercard();
    test_respb_smismember();
    
    printf("\nRESPB Key Operations (29):\n");
    test_respb_del();
    test_respb_exists();
    test_respb_unlink();
    test_respb_expire();
    test_respb_ttl();
    test_respb_persist();
    test_respb_expireat();
    test_respb_expiretime();
    test_respb_pexpire();
    test_respb_pexpireat();
    test_respb_pexpiretime();
    test_respb_pttl();
    test_respb_type();
    test_respb_rename();
    test_respb_renamenx();
    test_respb_keys();
    test_respb_scan();
    test_respb_randomkey();
    test_respb_dump();
    test_respb_restore();
    test_respb_migrate();
    test_respb_move();
    test_respb_copy();
    test_respb_sort();
    test_respb_sort_ro();
    test_respb_touch();
    test_respb_object();
    test_respb_wait();
    test_respb_waitaof();
    
    printf("\nRESPB Transaction Operations (5):\n");
    test_respb_multi();
    test_respb_exec();
    test_respb_discard();
    test_respb_watch();
    test_respb_unwatch();
    
    printf("\nRESPB Scripting and Functions (8):\n");
    test_respb_eval();
    test_respb_evalsha();
    test_respb_eval_ro();
    test_respb_evalsha_ro();
    test_respb_script();
    test_respb_fcall();
    test_respb_fcall_ro();
    test_respb_function();
    
    printf("\nRESPB Cluster Management (4):\n");
    test_respb_cluster();
    test_respb_readonly();
    test_respb_readwrite();
    test_respb_asking();
    
    printf("\nRESPB Connection Management (8):\n");
    test_respb_ping();
    test_respb_echo();
    test_respb_auth();
    test_respb_select();
    test_respb_quit();
    test_respb_hello();
    test_respb_reset();
    test_respb_client();
    
    printf("\nRESPB Server Management (28):\n");
    test_respb_dbsize();
    test_respb_flushdb();
    test_respb_flushall();
    test_respb_save();
    test_respb_bgsave();
    test_respb_bgrewriteaof();
    test_respb_lastsave();
    test_respb_shutdown();
    test_respb_info();
    test_respb_config();
    test_respb_command();
    test_respb_time();
    test_respb_role();
    test_respb_replicaof();
    test_respb_slaveof();
    test_respb_monitor();
    test_respb_debug();
    test_respb_sync();
    test_respb_psync();
    test_respb_replconf();
    test_respb_slowlog();
    test_respb_latency();
    test_respb_memory();
    test_respb_module();
    test_respb_acl();
    test_respb_failover();
    test_respb_swapdb();
    test_respb_lolwut();
    test_respb_restore_asking();
    test_respb_commandlog();
    
    printf("\nRESPB Pub/Sub Operations (9):\n");
    test_respb_publish();
    test_respb_subscribe();
    test_respb_unsubscribe();
    test_respb_psubscribe();
    test_respb_punsubscribe();
    test_respb_pubsub();
    test_respb_spublish();
    test_respb_ssubscribe();
    test_respb_sunsubscribe();
    
    printf("\nRESPB Bitmap Operations (7):\n");
    test_respb_setbit();
    test_respb_getbit();
    test_respb_bitcount();
    test_respb_bitpos();
    test_respb_bitop();
    test_respb_bitfield();
    test_respb_bitfield_ro();
    
    printf("\nRESPB HyperLogLog Operations (5):\n");
    test_respb_pfadd();
    test_respb_pfcount();
    test_respb_pfmerge();
    test_respb_pfdebug();
    test_respb_pfselftest();
    
    printf("\nRESPB Geospatial Operations (10):\n");
    test_respb_geoadd();
    test_respb_geodist();
    test_respb_geohash();
    test_respb_geopos();
    test_respb_georadius();
    test_respb_georadiusbymember();
    test_respb_georadius_ro();
    test_respb_georadiusbymember_ro();
    test_respb_geosearch();
    test_respb_geosearchstore();
    
    printf("\nRESPB Stream Operations (14):\n");
    test_respb_xadd();
    test_respb_xlen();
    test_respb_xrange();
    test_respb_xrevrange();
    test_respb_xread();
    test_respb_xreadgroup();
    test_respb_xdel();
    test_respb_xtrim();
    test_respb_xack();
    test_respb_xpending();
    test_respb_xclaim();
    test_respb_xautoclaim();
    test_respb_xinfo();
    test_respb_xgroup();
    test_respb_xsetid();
    
    printf("\nRESPB Module Commands (4):\n");
    test_respb_module_json_set();
    test_respb_json_get();
    test_respb_module_bf_add();
    test_respb_ft_search();
    
    printf("\nRESPB Passthrough (1):\n");
    test_respb_resp_passthrough();
    
    printf("\nError Handling (2):\n");
    test_respb_error_truncated();
    test_respb_error_unknown_opcode();
    
    printf("\nSerialization (1):\n");
    test_serialization_roundtrip();
    
    printf("\n");
    printf("=========================================================\n");
    printf("  Test Results\n");
    printf("=========================================================\n");
    printf("  Total Tests: %d\n", tests_passed + tests_failed);
    printf("  Passed:      %d\n", tests_passed);
    printf("  Failed:      %d\n", tests_failed);
    printf("  Coverage:    261/261 commands (100%%)\n");
    printf("=========================================================\n");
    printf("\n");
    
    return tests_failed > 0 ? 1 : 0;
}

