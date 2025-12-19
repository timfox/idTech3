/*
===============================================================================
Network message fuzzing harness for AFL/libFuzzer
===============================================================================
*/

#include "../src/qcommon/q_shared.h"
#include "../src/qcommon/msg.h"

// Mock implementations for fuzzing
void Com_Error(errorParm_t level, const char *error, ...) {
	(void)level;
	// Don't exit in fuzzing mode - just return
}

void Com_Printf(const char *fmt, ...) {
	(void)fmt;
	// Silent in fuzzing mode
}

#ifdef USE_LIBFUZZER
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	// Create a message buffer from the fuzzer input
	msg_t msg;
	byte buffer[MAX_MSGLEN];

	if (size > MAX_MSGLEN - 16) {
		return 0; // Skip oversized inputs
	}

	// Initialize message buffer
	MSG_Init(&msg, buffer, sizeof(buffer));

	// Write fuzzer data to message
	for (size_t i = 0; i < size; i++) {
		MSG_WriteByte(&msg, data[i]);
	}

	// Try to read various data types from the message
	// This exercises the message parsing code
	msg_t readMsg;
	MSG_Init(&readMsg, buffer, sizeof(buffer));
	readMsg.cursize = msg.cursize;

	// Try reading different types - this will test bounds checking
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
		case 4:
			MSG_ReadString(&readMsg);
			break;
		case 5:
			MSG_ReadStringLine(&readMsg);
			break;
		case 6:
			MSG_ReadData(&readMsg, 4);
			break;
		default:
			// Skip unknown commands
			break;
		}
	}

	return 0;
}

#else // AFL mode

#include <unistd.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
		return 1;
	}

	int fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	uint8_t buffer[MAX_MSGLEN];
	ssize_t size = read(fd, buffer, sizeof(buffer));
	close(fd);

	if (size < 0) {
		perror("read");
		return 1;
	}

	return LLVMFuzzerTestOneInput(buffer, size);
}

#endif // USE_LIBFUZZER
