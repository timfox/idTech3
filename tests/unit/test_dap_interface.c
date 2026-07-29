/*
 * Unit test: native id Tech 3 DAP protocol helpers.
 */
#include <stdio.h>
#include <string.h>

#include "dap_interface.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

int main(void) {
	char json[4096];
	char packet[4608];

	ASSERT(DAP_HandleJsonForTest("{\"seq\":7,\"type\":\"request\",\"command\":\"initialize\"}", json, sizeof(json)) == 1, "initialize response");
	ASSERT(strstr(json, "\"request_seq\":7") != NULL, "request seq echoed");
	ASSERT(strstr(json, "\"success\":true") != NULL, "initialize success");
	ASSERT(strstr(json, "supportsConfigurationDoneRequest") != NULL, "initialize capabilities");

	ASSERT(DAP_HandleJsonForTest("{\"seq\":8,\"type\":\"request\",\"command\":\"threads\"}", json, sizeof(json)) == 1, "threads response");
	ASSERT(strstr(json, "id Tech 3 main") != NULL, "thread name");

	ASSERT(DAP_HandleJsonForTest("{\"seq\":9,\"type\":\"request\",\"command\":\"setBreakpoints\"}", json, sizeof(json)) == 1, "unknown response");
	ASSERT(strstr(json, "\"success\":false") != NULL, "unknown command is explicit failure");

	ASSERT(DAP_IsAddressAllowedForTest("127.0.0.1", 0, "") == 1, "localhost allowed by default");
	ASSERT(DAP_IsAddressAllowedForTest("0.0.0.0", 0, "") == 0, "wildcard denied by default");
	ASSERT(DAP_IsAddressAllowedForTest("192.168.1.25", 1, "") == 0, "remote denied without token");
	ASSERT(DAP_IsAddressAllowedForTest("192.168.1.25", 1, "secret") == 1, "remote allowed with explicit token");

	ASSERT(DAP_HandleJsonWithTokenForTest("{\"seq\":10,\"type\":\"request\",\"command\":\"initialize\"}", "secret", json, sizeof(json)) == 1, "auth failure response");
	ASSERT(strstr(json, "\"success\":false") != NULL, "missing token rejected");
	ASSERT(strstr(json, "authentication failed") != NULL, "auth failure message");
	ASSERT(DAP_HandleJsonWithTokenForTest("{\"seq\":11,\"type\":\"request\",\"command\":\"initialize\",\"dapToken\":\"secret\"}", "secret", json, sizeof(json)) == 1, "auth success response");
	ASSERT(strstr(json, "\"request_seq\":11") != NULL, "authenticated request seq echoed");
	ASSERT(strstr(json, "\"success\":true") != NULL, "authenticated initialize success");

	ASSERT(DAP_BuildProtocolMessage("{\"ok\":true}", packet, sizeof(packet)) == 1, "protocol packet");
	ASSERT(strcmp(packet, "Content-Length: 11\r\n\r\n{\"ok\":true}") == 0, "protocol framing");

	printf("PASS: unit_dap_interface\n");
	return 0;
}
