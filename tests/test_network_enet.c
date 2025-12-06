/*
===========================================================================
ENet loopback smoke test (guarded by USE_ENET)
===========================================================================
*/

#include "test_framework.h"

#ifdef USE_ENET
#include "../libs/enet/include/enet/enet.h"

// Minimal Com_Printf stub for the test framework
void Com_Printf(const char *fmt, ...) {
	va_list argptr;
	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);
}

static qboolean wait_for_event(ENetHost *host, ENetEvent *event, ENetEventType type, int timeout_ms) {
	int waited = 0;
	const int step = 10;
	while (waited < timeout_ms) {
		if (enet_host_service(host, event, step) > 0) {
			if (event->type == type) {
				return qtrue;
			}
		}
		waited += step;
	}
	return qfalse;
}

TEST(enet_loopback_packet) {
	ASSERT_EQ(enet_initialize(), 0);

	ENetAddress serverAddr;
	serverAddr.host = ENET_HOST_ANY;
	serverAddr.port = 0; // auto-assign

	ENetHost *server = enet_host_create(&serverAddr, 1, 1, 0, 0);
	ASSERT_NOT_NULL(server);

	ENetAddress clientAddr;
	clientAddr.host = ENET_HOST_ANY;
	clientAddr.port = 0;
	ENetHost *client = enet_host_create(NULL, 1, 1, 0, 0);
	ASSERT_NOT_NULL(client);

	ENetAddress connectAddr = server->address;
	connectAddr.host = ENET_HOST_LOOPBACK;
	ENetPeer *peer = enet_host_connect(client, &connectAddr, 1, 0);
	ASSERT_NOT_NULL(peer);

	ENetEvent event;
	ASSERT_TRUE(wait_for_event(client, &event, ENET_EVENT_TYPE_CONNECT, 500));
	ASSERT_TRUE(wait_for_event(server, &event, ENET_EVENT_TYPE_CONNECT, 500));

	const char payload[] = "enet-loopback";
	ENetPacket *packet = enet_packet_create(payload, sizeof(payload), ENET_PACKET_FLAG_RELIABLE);
	ASSERT_NOT_NULL(packet);
	enet_peer_send(peer, 0, packet);
	enet_host_flush(client);

	qboolean received = qfalse;
	int waited = 0;
	while (waited < 500 && !received) {
		if (enet_host_service(server, &event, 10) > 0 && event.type == ENET_EVENT_TYPE_RECEIVE) {
			ASSERT_EQ(event.packet->dataLength, sizeof(payload));
			ASSERT_TRUE(memcmp(event.packet->data, payload, sizeof(payload)) == 0);
			enet_packet_destroy(event.packet);
			received = qtrue;
			break;
		}
		waited += 10;
	}
	ASSERT_TRUE(received);

	enet_peer_disconnect(peer, 0);
	wait_for_event(server, &event, ENET_EVENT_TYPE_DISCONNECT, 250);
	wait_for_event(client, &event, ENET_EVENT_TYPE_DISCONNECT, 250);

	enet_host_destroy(client);
	enet_host_destroy(server);
	enet_deinitialize();
}

int main(void) {
	Com_Printf("Running ENet loopback test...\n\n");
	RUN_TEST(enet_loopback_packet);
	PRINT_TEST_SUMMARY();
	return (test_failed > 0) ? 1 : 0;
}

#else
int main(void) {
	printf("ENet not enabled; skipping tests.\n");
	return 0;
}
#endif

