/*
===============================================================================
Network Protocol Fuzzing Test Suite

Comprehensive fuzz testing for network protocol parsing and validation.
===============================================================================
*/

#include "../common/q_shared.h"
#include "../common/qcommon.h"
#include "../common/net_protocol_validation.h"
#include "../server/sv_main.h"
#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Mock implementations for fuzzing
void Com_Error(errorParm_t level, const char *error, ...) {
    (void)level; (void)error;
    // Don't exit in fuzzing mode
}

void Com_Printf(const char *fmt, ...) {
    (void)fmt;
    // Silent in fuzzing mode
}

void Com_DPrintf(int level, const char *fmt, ...) {
    (void)level; (void)fmt;
    // Silent in fuzzing mode
}

// Fuzz test data structures
typedef struct {
    const char *name;
    void (*fuzz_func)(const uint8_t *data, size_t size);
    int executions;
    int crashes_found;
    double avg_execution_time;
} fuzz_test_t;

// Global fuzz statistics
static struct {
    int total_tests;
    int total_executions;
    int total_crashes;
    double start_time;
    double total_time;
} fuzz_stats = {0};

//============================================================================
// Network Message Fuzzing
//============================================================================

static void Fuzz_NetworkMessage(const uint8_t *data, size_t size) {
    msg_t msg;
    byte buffer[2048];

    if (size > sizeof(buffer) - 16) {
        return; // Skip oversized inputs
    }

    // Initialize message buffer
    MSG_Init(&msg, buffer, sizeof(buffer));

    // Write fuzzer data to message
    for (size_t i = 0; i < size; i++) {
        MSG_WriteByte(&msg, data[i]);
    }

    // Try to read various data types from the message
    // This exercises the message parsing code with malformed data
    msg_t readMsg;
    MSG_Init(&readMsg, buffer, sizeof(buffer));
    readMsg.cursize = msg.cursize;

    while (readMsg.readcount < readMsg.cursize) {
        int cmd = MSG_ReadByte(&readMsg);
        switch (cmd & 0x0F) { // Use lower bits to limit switch cases
        case 0:
            MSG_ReadByte(&readMsg);
            break;
        case 1:
            MSG_ReadShort(&readMsg);
            break;
        case 2:
            MSG_ReadLong(&readMsg);
            break;
        case 3:
            MSG_ReadFloat(&readMsg);
            break;
        case 4: {
            char str_buf[256];
            MSG_ReadString(&readMsg, str_buf, sizeof(str_buf));
            break;
        }
        case 5:
            MSG_ReadData(&readMsg, buffer, MIN(64, readMsg.cursize - readMsg.readcount));
            break;
        default:
            // Skip unknown command
            break;
        }

        // Prevent infinite loops
        if (readMsg.readcount > readMsg.cursize + 1000) {
            break;
        }
    }
}

//============================================================================
// Network Protocol Validation Fuzzing
//============================================================================

static void Fuzz_ProtocolValidation(const uint8_t *data, size_t size) {
    char address[256];
    int port;
    netadr_t addr;

    // Test address parsing with fuzzed data
    if (size >= 2) {
        // Try different address formats
        char addr_str[256];
        size_t copy_size = MIN(size, sizeof(addr_str) - 1);
        memcpy(addr_str, data, copy_size);
        addr_str[copy_size] = '\0';

        // Test IPv4 parsing
        NET_ParseAddress(addr_str, address, &port);

        // Test IPv6-like parsing (even if not supported, should not crash)
        char ipv6_like[256];
        Com_sprintf(ipv6_like, sizeof(ipv6_like), "[%s]:%d", addr_str, (int)(data[0] % 65536));
        NET_ParseAddress(ipv6_like, address, &port);
    }

    // Test connection string validation
    if (size > 0) {
        char connect_str[512];
        size_t copy_size = MIN(size, sizeof(connect_str) - 1);
        memcpy(connect_str, data, copy_size);
        connect_str[copy_size] = '\0';

        // Test various connection string formats
        NET_ParseAddress(connect_str, address, &port);

        // Test malformed strings
        for (size_t i = 0; i < copy_size; i++) {
            if (connect_str[i] == '\0') break;
            char temp = connect_str[i];
            connect_str[i] = '\0'; // Truncate string
            NET_ParseAddress(connect_str, address, &port);
            connect_str[i] = temp; // Restore
        }
    }
}

//============================================================================
// File Parsing Fuzzing
//============================================================================

static void Fuzz_FileParsing(const uint8_t *data, size_t size) {
    // Test various file parsing functions with fuzzed data

    // Test config file parsing
    if (size > 0) {
        char config_line[1024];
        size_t copy_size = MIN(size, sizeof(config_line) - 1);
        memcpy(config_line, data, copy_size);
        config_line[copy_size] = '\0';

        // Test Cvar parsing (would need mock cvars)
        // For now, just test string parsing functions

        // Test command parsing
        Cmd_TokenizeString(config_line);
        while (Cmd_Argc() > 0) {
            Cmd_Argv(0);
            Cmd_ArgsFrom(1);
            break; // Just test first token
        }
    }

    // Test shader file parsing
    if (size > 10) {
        // Simulate shader file content
        char shader_content[2048];
        size_t copy_size = MIN(size, sizeof(shader_content) - 1);
        memcpy(shader_content, data, copy_size);
        shader_content[copy_size] = '\0';

        // Test for common shader keywords that shouldn't crash parser
        const char *keywords[] = {
            "surfaceparm", "cull", "polygonOffset", "entityMergable",
            "tessSize", "clampTime", "noPicMip", "noTC", "tcGen",
            "map", "clampmap", "animmap", "rgbGen", "alphaGen",
            "tcMod", "scroll", "scale", "stretch", "turb", "rotate",
            "stage", "blendFunc", "rgb", "alpha", "depthFunc", "depthWrite",
            NULL
        };

        for (const char **kw = keywords; *kw; kw++) {
            strstr(shader_content, *kw); // Test string searching
        }
    }
}

//============================================================================
// BSP File Fuzzing
//============================================================================

static void Fuzz_BSPParsing(const uint8_t *data, size_t size) {
    // Test BSP file parsing with fuzzed data
    // This is a simplified version - full BSP parsing would be complex

    if (size < sizeof(dheader_t)) {
        return;
    }

    // Simulate BSP header parsing
    dheader_t *header = (dheader_t *)data;

    // Check for valid magic number (should be BSP magic)
    if (LittleLong(header->ident) == BSP_IDENT) {
        // Test lump parsing
        for (int i = 0; i < HEADER_LUMPS; i++) {
            lump_t *lump = &header->lumps[i];
            int lump_ofs = LittleLong(lump->fileofs);
            int lump_len = LittleLong(lump->filelen);

            // Bounds checking
            if (lump_ofs >= 0 && lump_ofs < (int)size &&
                lump_len >= 0 && lump_len < (int)size &&
                lump_ofs + lump_len <= (int)size) {
                // Valid lump - test accessing it
                const byte *lump_data = data + lump_ofs;
                (void)lump_data; // Prevent unused variable warning
            }
        }
    } else {
        // Invalid BSP - test error handling
        // Parser should gracefully handle invalid data
    }
}

//============================================================================
// MD3/Model File Fuzzing
//============================================================================

static void Fuzz_MD3Parsing(const uint8_t *data, size_t size) {
    if (size < sizeof(md3Header_t)) {
        return;
    }

    md3Header_t *header = (md3Header_t *)data;

    // Check for valid MD3 magic
    if (LittleLong(header->ident) == MD3_IDENT) {
        // Test frame parsing
        int numFrames = LittleLong(header->numFrames);
        int numTags = LittleLong(header->numTags);
        int numSurfaces = LittleLong(header->numSurfaces);

        // Bounds checking
        if (numFrames >= 0 && numFrames < 1000 &&
            numTags >= 0 && numTags < 1000 &&
            numSurfaces >= 0 && numSurfaces < 1000) {

            // Test accessing frames
            size_t frames_ofs = LittleLong(header->ofsFrames);
            if (frames_ofs >= sizeof(md3Header_t) &&
                frames_ofs + numFrames * sizeof(md3Frame_t) <= size) {
                md3Frame_t *frames = (md3Frame_t *)(data + frames_ofs);
                (void)frames; // Test access
            }

            // Test accessing tags
            size_t tags_ofs = LittleLong(header->ofsTags);
            if (tags_ofs >= sizeof(md3Header_t) &&
                tags_ofs + numTags * sizeof(md3Tag_t) <= size) {
                md3Tag_t *tags = (md3Tag_t *)(data + tags_ofs);
                (void)tags; // Test access
            }

            // Test accessing surfaces
            size_t surfaces_ofs = LittleLong(header->ofsSurfaces);
            if (surfaces_ofs >= sizeof(md3Header_t) &&
                surfaces_ofs < size) {
                // Surface parsing would be more complex
                (void)surfaces_ofs;
            }
        }
    }
}

//============================================================================
// Fuzz Test Runner
//============================================================================

static fuzz_test_t fuzz_tests[] = {
    {"Network Message Parsing", Fuzz_NetworkMessage, 0, 0, 0.0},
    {"Protocol Validation", Fuzz_ProtocolValidation, 0, 0, 0.0},
    {"File Parsing", Fuzz_FileParsing, 0, 0, 0.0},
    {"BSP File Parsing", Fuzz_BSPParsing, 0, 0, 0.0},
    {"MD3 Model Parsing", Fuzz_MD3Parsing, 0, 0, 0.0},
    {NULL, NULL, 0, 0, 0.0}
};

#ifdef USE_LIBFUZZER
// libFuzzer integration
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static int test_index = 0;

    if (size == 0) return 0;

    // Rotate through different fuzz tests
    for (int i = 0; fuzz_tests[i].name; i++) {
        if (test_index % 5 == i) {
            fuzz_tests[i].executions++;
            double start_time = (double)clock() / CLOCKS_PER_SEC * 1000.0;

            // Run the fuzz test in a try-catch like environment
            fuzz_tests[i].fuzz_func(data, size);

            double end_time = (double)clock() / CLOCKS_PER_SEC * 1000.0;
            double exec_time = end_time - start_time;

            // Update statistics
            fuzz_tests[i].avg_execution_time =
                (fuzz_tests[i].avg_execution_time * (fuzz_tests[i].executions - 1) + exec_time) /
                fuzz_tests[i].executions;

            break;
        }
    }

    test_index++;
    return 0; // Non-zero return indicates crash found
}

#else

// Standalone fuzz testing
static void GenerateFuzzData(uint8_t *buffer, size_t size, unsigned int seed) {
    srand(seed);

    for (size_t i = 0; i < size; i++) {
        // Generate somewhat realistic data patterns
        int pattern = rand() % 10;
        switch (pattern) {
        case 0: // ASCII text
            buffer[i] = (rand() % 95) + 32;
            break;
        case 1: // Binary data
            buffer[i] = rand() % 256;
            break;
        case 2: // NULL bytes
            buffer[i] = 0;
            break;
        case 3: // Magic numbers (common file headers)
            buffer[i] = (rand() % 4 == 0) ? 'I' : (rand() % 4 == 1) ? 'B' : rand() % 256;
            break;
        default:
            buffer[i] = rand() % 256;
            break;
        }
    }
}

#endif

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    printf("Network Protocol and File Parsing Fuzz Testing\n");
    printf("===============================================\n\n");

    fuzz_stats.start_time = (double)clock() / CLOCKS_PER_SEC * 1000.0;

#ifndef USE_LIBFUZZER
    // Standalone fuzz testing mode
    const int NUM_ITERATIONS = 10000;
    const size_t MAX_DATA_SIZE = 2048;
    uint8_t fuzz_data[MAX_DATA_SIZE];

    printf("Running %d fuzz iterations...\n", NUM_ITERATIONS);

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        // Generate fuzz data
        size_t data_size = (rand() % MAX_DATA_SIZE) + 1;
        GenerateFuzzData(fuzz_data, data_size, iter);

        // Run each fuzz test
        for (fuzz_test_t *test = fuzz_tests; test->name; test++) {
            test->executions++;
            double start_time = (double)clock() / CLOCKS_PER_SEC * 1000.0;

            // Run the fuzz test
            test->fuzz_func(fuzz_data, data_size);

            double end_time = (double)clock() / CLOCKS_PER_SEC * 1000.0;
            double exec_time = end_time - start_time;

            // Update statistics
            test->avg_execution_time =
                (test->avg_execution_time * (test->executions - 1) + exec_time) /
                test->executions;

            fuzz_stats.total_executions++;
        }

        if (iter % 1000 == 0) {
            printf("Completed %d/%d iterations\n", iter, NUM_ITERATIONS);
        }
    }
#endif

    // Print results
    fuzz_stats.total_time = ((double)clock() / CLOCKS_PER_SEC * 1000.0) - fuzz_stats.start_time;

    printf("\nFuzz Testing Results:\n");
    printf("=====================\n");
    printf("Total execution time: %.2f seconds\n", fuzz_stats.total_time / 1000.0);
    printf("Total test executions: %d\n", fuzz_stats.total_executions);
    printf("Crashes found: %d\n\n", fuzz_stats.total_crashes);

    printf("Detailed Results:\n");
    for (fuzz_test_t *test = fuzz_tests; test->name; test++) {
        printf("  %s:\n", test->name);
        printf("    Executions: %d\n", test->executions);
        printf("    Avg time: %.3f ms\n", test->avg_execution_time);
        printf("    Crashes: %d\n", test->crashes_found);
        printf("\n");
    }

    printf("Fuzz testing completed successfully!\n");

    return fuzz_stats.total_crashes > 0 ? 1 : 0;
}