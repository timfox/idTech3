#ifndef NET_P2P_TURN_AUTH_H
#define NET_P2P_TURN_AUTH_H

#include "q_shared.h"

qboolean NET_P2P_TurnAuthAvailable( void );
int NET_P2P_TurnAppendMessageIntegrity( byte *packet, int len, int maxLen, const char *username, const char *password, const char *realm, const char *nonce );

#endif /* NET_P2P_TURN_AUTH_H */
