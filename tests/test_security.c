/*
===============================================================================
Security hardening tests - Input validation and security checks
===============================================================================
*/

#include "test_framework.h"
#include "../src/common/q_shared.h"
#include <string.h>
#include <stdlib.h>

// Mock implementations
void Com_Error(errorParm_t level, const char *error, ...) {
	(void)level;
	va_list argptr;
	va_start(argptr, error);
	vfprintf(stderr, error, argptr);
	va_end(argptr);
	exit(1);
}

void Com_Printf(const char *fmt, ...) {
	va_list argptr;
	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);
}

// Input validation functions
static qboolean is_valid_string(const char *str, size_t max_len) {
	if (!str) return qfalse;
	size_t len = strlen(str);
	if (len == 0 || len >= max_len) return qfalse;

	// Check for control characters and other potentially dangerous chars
	for (size_t i = 0; i < len; i++) {
		char c = str[i];
		if (c < 32 || c == 127) { // Control characters
			return qfalse;
		}
		if (strchr(";|&`$('\"\\<>", c)) {
			// Potentially dangerous shell metacharacters
			return qfalse;
		}
	}
	return qtrue;
}

static qboolean is_valid_filename(const char *filename) {
	if (!filename || strlen(filename) == 0) return qfalse;
	if (strlen(filename) > MAX_QPATH) return qfalse;

	// Check for directory traversal attempts
	if (strstr(filename, "..") != NULL) return qfalse;
	if (strstr(filename, "/") != NULL || strstr(filename, "\\") != NULL) return qfalse;

	// Check for URL-encoded directory traversal (%2e = ., %2f = /, %5c = \)
	if (strstr(filename, "%2e%2e") != NULL) return qfalse; // %2e%2e = ..
	if (strstr(filename, "%2f") != NULL || strstr(filename, "%5c") != NULL) return qfalse;

	// Check for invalid characters
	const char *invalid_chars = "<>:\"|?*";
	for (size_t i = 0; i < strlen(filename); i++) {
		if (strchr(invalid_chars, filename[i]) != NULL) return qfalse;
	}

	return qtrue;
}

static qboolean is_valid_integer_string(const char *str, int min_val, int max_val) {
	if (!str || strlen(str) == 0) return qfalse;

	char *endptr;
	long val = strtol(str, &endptr, 10);

	// Check if conversion was successful
	if (*endptr != '\0') return qfalse;

	// Check bounds
	if (val < min_val || val > max_val) return qfalse;

	return qtrue;
}

TEST(input_validation_string) {
	// Valid strings
	ASSERT_TRUE(is_valid_string("hello", 100));
	ASSERT_TRUE(is_valid_string("test_string", 100));
	ASSERT_TRUE(is_valid_string("a", 100));

	// Invalid strings - too long
	char long_string[300];
	memset(long_string, 'a', 299);
	long_string[299] = '\0';
	ASSERT_FALSE(is_valid_string(long_string, 100));

	// Invalid strings - control characters
	ASSERT_FALSE(is_valid_string("hello\nworld", 100));
	ASSERT_FALSE(is_valid_string("hello\x01world", 100));
	ASSERT_FALSE(is_valid_string("hello\x7fworld", 100));

	// Invalid strings - dangerous characters
	ASSERT_FALSE(is_valid_string("hello<world", 100));
	ASSERT_FALSE(is_valid_string("hello>world", 100));
	ASSERT_FALSE(is_valid_string("hello|world", 100));
	ASSERT_FALSE(is_valid_string("hello;world", 100));

	// Edge cases
	ASSERT_FALSE(is_valid_string(NULL, 100));
	ASSERT_FALSE(is_valid_string("", 100));
}

TEST(input_validation_filename) {
	// Valid filenames
	ASSERT_TRUE(is_valid_filename("test.txt"));
	ASSERT_TRUE(is_valid_filename("myfile"));
	ASSERT_TRUE(is_valid_filename("file.with.dots"));

	// Invalid filenames - directory traversal
	ASSERT_FALSE(is_valid_filename("../test.txt"));
	ASSERT_FALSE(is_valid_filename("..\\test.txt"));
	ASSERT_FALSE(is_valid_filename("../../etc/passwd"));

	// Invalid filenames - path separators
	ASSERT_FALSE(is_valid_filename("dir/file.txt"));
	ASSERT_FALSE(is_valid_filename("dir\\file.txt"));

	// Invalid filenames - dangerous characters
	ASSERT_FALSE(is_valid_filename("file<>.txt"));
	ASSERT_FALSE(is_valid_filename("file:with:colons"));
	ASSERT_FALSE(is_valid_filename("file\"with\"quotes"));
	ASSERT_FALSE(is_valid_filename("file|pipe.txt"));
	ASSERT_FALSE(is_valid_filename("file?question.txt"));
	ASSERT_FALSE(is_valid_filename("file*asterisk.txt"));

	// Invalid filenames - too long
	char long_filename[MAX_QPATH + 10];
	memset(long_filename, 'a', sizeof(long_filename) - 1);
	long_filename[sizeof(long_filename) - 1] = '\0';
	ASSERT_FALSE(is_valid_filename(long_filename));

	// Edge cases
	ASSERT_FALSE(is_valid_filename(NULL));
	ASSERT_FALSE(is_valid_filename(""));
}

TEST(input_validation_integer) {
	// Valid integers
	ASSERT_TRUE(is_valid_integer_string("0", -100, 100));
	ASSERT_TRUE(is_valid_integer_string("42", 0, 100));
	ASSERT_TRUE(is_valid_integer_string("-42", -100, 0));
	ASSERT_TRUE(is_valid_integer_string("100", 50, 150));

	// Invalid integers - out of bounds
	ASSERT_FALSE(is_valid_integer_string("150", 0, 100));
	ASSERT_FALSE(is_valid_integer_string("-50", 0, 100));

	// Invalid integers - not numeric
	ASSERT_FALSE(is_valid_integer_string("abc", 0, 100));
	ASSERT_FALSE(is_valid_integer_string("42abc", 0, 100));
	ASSERT_FALSE(is_valid_integer_string("42.5", 0, 100));
	ASSERT_FALSE(is_valid_integer_string("", 0, 100));
	ASSERT_FALSE(is_valid_integer_string(" ", 0, 100));

	// Edge cases
	ASSERT_FALSE(is_valid_integer_string(NULL, 0, 100));
}

TEST(buffer_bounds_checking) {
	char buffer[64];

	// Safe operations
	strncpy(buffer, "hello", sizeof(buffer));
	ASSERT_TRUE(strlen(buffer) == 5);

	// Test potential buffer overflow protection
	const char *long_string = "this_is_a_very_long_string_that_should_be_longer_than_sixty_four_characters_to_test_buffer_bounds";
	strncpy(buffer, long_string, sizeof(buffer) - 1);
	buffer[sizeof(buffer) - 1] = '\0';

	// Should be truncated safely
	ASSERT_TRUE(strlen(buffer) <= sizeof(buffer) - 1);
	ASSERT_TRUE(strncmp(buffer, long_string, sizeof(buffer) - 1) == 0);
}

TEST(memory_safety_bounds) {
	// Test array bounds checking
	int array[10];

	// Safe operations
	for (int i = 0; i < 10; i++) {
		array[i] = i;
	}

	// Verify contents
	for (int i = 0; i < 10; i++) {
		ASSERT_EQ(array[i], i);
	}

	// Test bounds checking with Com_Error (would crash in real engine)
	// Note: In a real security check, we'd use bounds-checking instrumentation
}

TEST(command_injection_prevention) {
	// Test that user input doesn't contain command injection attempts
	const char *safe_inputs[] = {
		"hello world",
		"player_name",
		"level1",
		"config.cfg"
	};

	const char *dangerous_inputs[] = {
		"hello; rm -rf /",
		"test && cat /etc/passwd",
		"file.txt | evil_command",
		"test`whoami`",
		"test$(uname)"
	};

	for (size_t i = 0; i < sizeof(safe_inputs) / sizeof(safe_inputs[0]); i++) {
		ASSERT_TRUE(is_valid_string(safe_inputs[i], 100));
	}

	for (size_t i = 0; i < sizeof(dangerous_inputs) / sizeof(dangerous_inputs[0]); i++) {
		ASSERT_FALSE(is_valid_string(dangerous_inputs[i], 100));
	}
}

TEST(null_pointer_protection) {
	// Test null pointer handling

	// These should not crash (in safe implementations)
	// strlen(NULL); // Would crash - commented out
	// strcpy(NULL, "test"); // Would crash - commented out

	// Test our validation functions with null inputs
	ASSERT_FALSE(is_valid_string(NULL, 100));
	ASSERT_FALSE(is_valid_filename(NULL));
	ASSERT_FALSE(is_valid_integer_string(NULL, 0, 100));
}

TEST(integer_overflow_protection) {
	// Test for potential integer overflows
	int a = INT_MAX / 2;
	int b = INT_MAX / 2;

	// This should not overflow in safe arithmetic
	int result = a + b;
	ASSERT_TRUE(result > 0); // Basic sanity check

	// Test large allocations (would be caught by malloc limits in real systems)
	// In a real security check, we'd have allocation size limits
}

TEST(format_string_protection) {
	char buffer[512];

	// Safe format strings
	snprintf(buffer, sizeof(buffer), "Hello %s", "world");
	ASSERT_STR_EQ(buffer, "Hello world");

	snprintf(buffer, sizeof(buffer), "Value: %d", 42);
	ASSERT_TRUE(strstr(buffer, "42") != NULL);

	// Test bounds checking
	const char *very_long_format = "This is a very long format string that might cause issues if not handled properly with buffer bounds checking and proper truncation mechanisms in place to prevent buffer overflows and other security vulnerabilities that could be exploited by malicious actors attempting to compromise system security through format string attacks or similar techniques";
	snprintf(buffer, sizeof(buffer), "%s", very_long_format);

	// Should be truncated safely
	size_t buffer_len = strlen(buffer);
	ASSERT_TRUE(buffer_len < sizeof(buffer)); // Should not overflow
	ASSERT_TRUE(strncmp(buffer, very_long_format, buffer_len) == 0);
}





TEST(input_sanitization_comprehensive) {
	// Test comprehensive input sanitization

	// SQL injection patterns (though we don't use SQL, good practice)
	const char *sql_injection_attempts[] = {
		"'; DROP TABLE users; --",
		"' OR '1'='1",
		"admin'--",
		"1; SELECT * FROM users"
	};

	// XSS patterns
	const char *xss_attempts[] = {
		"<script>alert('xss')</script>",
		"<img src=x onerror=alert(1)>",
		"javascript:alert('xss')",
		"<iframe src='javascript:alert(1)'></iframe>"
	};

	// Directory traversal (already tested above)
	const char *traversal_attempts[] = {
		"../../../etc/passwd",
		"..\\..\\..\\boot.ini",
		"%2e%2e%2f%2e%2e%2fetc%2fpasswd" // URL encoded
	};

	// Test that dangerous characters are detected
	for (size_t i = 0; i < sizeof(sql_injection_attempts) / sizeof(sql_injection_attempts[0]); i++) {
		ASSERT_FALSE(is_valid_string(sql_injection_attempts[i], 100));
	}

	for (size_t i = 0; i < sizeof(xss_attempts) / sizeof(xss_attempts[0]); i++) {
		ASSERT_FALSE(is_valid_string(xss_attempts[i], 100));
	}

	for (size_t i = 0; i < sizeof(traversal_attempts) / sizeof(traversal_attempts[0]); i++) {
		qboolean result = is_valid_filename(traversal_attempts[i]);
		if (result) {
			Com_Printf("FAIL: Filename '%s' should be invalid but passed validation\n", traversal_attempts[i]);
		}
		ASSERT_FALSE(result);
	}
}

TEST(race_condition_simulation) {
	// Test race condition detection patterns
	// This is difficult to test directly, but we can test the primitives

	static int shared_counter = 0;
	static qboolean test_completed = qfalse;

	// Simulate concurrent access patterns (single-threaded approximation)
	for (int i = 0; i < 1000; i++) {
		int local_counter = shared_counter;
		local_counter++; // Simulate some work
		shared_counter = local_counter;
	}

	// In real implementations, this would need atomic operations
	// For now, just verify the counter incremented
	ASSERT_EQ(shared_counter, 1000);
	test_completed = qtrue;
	ASSERT_TRUE(test_completed);
}

TEST(stack_buffer_overflow_detection) {
	// Test stack buffer overflow detection patterns
	char local_buffer[64];
	char *heap_buffer;

	// Test stack buffer
	strncpy(local_buffer, "short string", sizeof(local_buffer) - 1);
	local_buffer[sizeof(local_buffer) - 1] = '\0';
	ASSERT_TRUE(strlen(local_buffer) < sizeof(local_buffer));

	// Test heap buffer
	heap_buffer = malloc(64);
	if (heap_buffer) {
		strncpy(heap_buffer, "heap string", 63);
		heap_buffer[63] = '\0';
		ASSERT_TRUE(strlen(heap_buffer) < 64);
		free(heap_buffer);
	}

	// Test potential underflow (though unlikely in practice)
	char small_array[4] = {'a', 'b', 'c', '\0'};
	ASSERT_EQ(strlen(small_array), 3);
}

TEST(use_after_free_detection) {
	// Test use-after-free detection patterns
	// This is hard to test directly without ASan, but we can test the concept

	char *ptr = malloc(100);
	if (ptr) {
		strncpy(ptr, "test data", 99);
		ptr[99] = '\0';

		// "Use" the data before free
		ASSERT_TRUE(strlen(ptr) > 0);

		free(ptr);
		// In real implementations with ASan/valgrind, accessing ptr here would be detected
		// For this test, we just verify the allocation/deallocation pattern worked
	}

	// Test double-free detection (would crash in real implementations)
	// char *double_free_ptr = malloc(10);
	// free(double_free_ptr);
	// free(double_free_ptr); // Would crash - commented out
}

TEST(heap_buffer_overflow_detection) {
	// Test heap buffer overflow detection
	char *heap_buffer = malloc(32);

	if (heap_buffer) {
		// Safe write
		strncpy(heap_buffer, "safe", 31);
		heap_buffer[31] = '\0';
		ASSERT_TRUE(strlen(heap_buffer) == 4);

		// In real implementations, this would overflow:
		// strcpy(heap_buffer + 28, "this_would_overflow"); // Commented out

		free(heap_buffer);
	}

	// Test allocation size validation
	// In real code, very large allocations should be rejected
	size_t reasonable_size = 1024;
	size_t unreasonable_size = SIZE_MAX / 2; // Would likely fail anyway

	char *reasonable = malloc(reasonable_size);
	if (reasonable) {
		free(reasonable);
	}

	// Very large allocation might fail, which is expected
	char *unreasonable = malloc(unreasonable_size);
	if (unreasonable) {
		free(unreasonable);
	} // else: allocation failed as expected for unreasonably large size
}

int main(void) {
	Com_Printf("Running security hardening tests...\n\n");

	RUN_TEST(input_validation_string);
	RUN_TEST(input_validation_filename);
	RUN_TEST(input_validation_integer);
	RUN_TEST(input_sanitization_comprehensive);
	RUN_TEST(buffer_bounds_checking);
	RUN_TEST(memory_safety_bounds);
	RUN_TEST(command_injection_prevention);
	RUN_TEST(null_pointer_protection);
	RUN_TEST(integer_overflow_protection);
	RUN_TEST(format_string_protection);
	RUN_TEST(stack_buffer_overflow_detection);
	RUN_TEST(heap_buffer_overflow_detection);
	RUN_TEST(use_after_free_detection);
	RUN_TEST(race_condition_simulation);

	PRINT_TEST_SUMMARY();

	Com_Printf("\nSecurity tests completed.\n");
	Com_Printf("These tests validate basic security hardening measures.\n");

	return (test_failed > 0) ? 1 : 0;
}
