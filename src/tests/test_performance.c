/*
=============================================================================
Performance Benchmarking Suite

Basic performance testing for core operations.
=============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>

// Simple timing function
double get_time_ms(void) {
    return (double)clock() / CLOCKS_PER_SEC * 1000.0;
}

// Benchmark configuration
#define ITERATIONS 1000

// Vector operations
typedef float vec3_t[3];

void VectorAdd(const vec3_t a, const vec3_t b, vec3_t out) {
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

void VectorScale(const vec3_t in, float scale, vec3_t out) {
    out[0] = in[0] * scale;
    out[1] = in[1] * scale;
    out[2] = in[2] * scale;
}

float VectorLength(const vec3_t v) {
    return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

// Matrix operations
typedef float mat4_t[16];

void MatrixMultiply(const mat4_t a, const mat4_t b, mat4_t out) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            out[i*4 + j] = 0;
            for (int k = 0; k < 4; k++) {
                out[i*4 + j] += a[i*4 + k] * b[k*4 + j];
            }
        }
    }
}

int main(void) {
    printf("Running basic performance tests...\n");

    // Test 1: Vector operations
    printf("  Testing vector operations...\n");
    vec3_t a = {1.0f, 2.0f, 3.0f};
    vec3_t b = {4.0f, 5.0f, 6.0f};
    vec3_t result;

    double start = get_time_ms();
    for (int i = 0; i < ITERATIONS; i++) {
        VectorAdd(a, b, result);
        VectorScale(result, 2.0f, result);
        float len = VectorLength(result);
        (void)len; // Suppress unused variable warning
    }
    double end = get_time_ms();

    printf("  ✅ Vector operations: %.2f ms for %d iterations\n", end - start, ITERATIONS);

    // Test 2: Matrix operations
    printf("  Testing matrix operations...\n");
    mat4_t mat1 = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    mat4_t mat2 = {2,0,0,0, 0,2,0,0, 0,0,2,0, 0,0,0,2};
    mat4_t matResult;

    start = get_time_ms();
    for (int i = 0; i < ITERATIONS / 10; i++) { // Fewer iterations for matrix ops
        MatrixMultiply(mat1, mat2, matResult);
    }
    end = get_time_ms();

    printf("  ✅ Matrix operations: %.2f ms for %d iterations\n", end - start, ITERATIONS / 10);

    // Test 3: Memory operations
    printf("  Testing memory operations...\n");
    start = get_time_ms();
    for (int i = 0; i < ITERATIONS / 100; i++) { // Even fewer for memory ops
        void* ptr = malloc(1024);
        memset(ptr, 0xAA, 1024);
        free(ptr);
    }
    end = get_time_ms();

    printf("  ✅ Memory operations: %.2f ms for %d iterations\n", end - start, ITERATIONS / 100);

    printf("Performance tests completed successfully\n");
    return 0;
}
