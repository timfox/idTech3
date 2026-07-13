#ifndef NET_OSCAR_RAW_H
#define NET_OSCAR_RAW_H

#include "q_shared.h"
#include "net_oscar.h"

#define OSCAR_RAW_MAX_FRAME 8192
#define OSCAR_RAW_MAX_COOKIE 512

typedef enum {
	OSCAR_RAW_FLAP_SIGNON = 0x01,
	OSCAR_RAW_FLAP_DATA = 0x02,
	OSCAR_RAW_FLAP_ERROR = 0x03,
	OSCAR_RAW_FLAP_SIGNOFF = 0x04,
	OSCAR_RAW_FLAP_KEEPALIVE = 0x05
} oscarRawFlapChannel_t;

typedef struct {
	byte channel;
	unsigned short sequence;
	unsigned short payloadLen;
	const byte *payload;
} oscarRawFlapFrame_t;

typedef struct {
	unsigned short family;
	unsigned short subtype;
	unsigned short flags;
	unsigned int requestId;
	const byte *body;
	int bodyLen;
} oscarRawSnac_t;

typedef struct {
	char bosHost[128];
	int bosPort;
	byte cookie[OSCAR_RAW_MAX_COOKIE];
	int cookieLen;
	unsigned short errorCode;
} oscarRawAuthReply_t;

int OSCAR_RawBuildFlap( byte channel, unsigned short sequence, const byte *payload, int payloadLen, byte *out, int outSize );
int OSCAR_RawBuildLoginSignon( unsigned short sequence, const char *screenName, const char *password, byte *out, int outSize );
int OSCAR_RawBuildCookieSignon( unsigned short sequence, const byte *cookie, int cookieLen, byte *out, int outSize );
int OSCAR_RawBuildSnac( unsigned short sequence, unsigned short family, unsigned short subtype, unsigned int requestId,
                        const byte *body, int bodyLen, byte *out, int outSize );
int OSCAR_RawBuildClientOnline( unsigned short sequence, unsigned int requestId, byte *out, int outSize );
int OSCAR_RawBuildIM( unsigned short sequence, unsigned int requestId, const char *screenName, const char *text, byte *out, int outSize );
int OSCAR_RawBuildPresence( unsigned short sequence, unsigned int requestId, const char *status, byte *out, int outSize );

qboolean OSCAR_RawParseFlap( const byte *data, int dataLen, oscarRawFlapFrame_t *frame, int *consumed );
qboolean OSCAR_RawParseSnac( const byte *payload, int payloadLen, oscarRawSnac_t *snac );
qboolean OSCAR_RawParseAuthReply( const byte *payload, int payloadLen, oscarRawAuthReply_t *reply );
qboolean OSCAR_RawParseIncomingIM( const oscarRawSnac_t *snac, oscarEvent_t *eventOut );

#endif /* NET_OSCAR_RAW_H */
