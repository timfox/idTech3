/*
===========================================================================
Native Debug Adapter Protocol transport for id Tech 3.

This is engine tooling, not gameplay. Games can later register debug providers,
but the TCP/DAP framing and lifecycle live in the engine.
===========================================================================
*/

#include "dap_interface.h"

extern "C" {
#include "q_shared.h"
#include "qcommon.h"
}

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#ifdef _WIN32
#include <WinSock2.h>
#include <ws2tcpip.h>
typedef SOCKET dapSocket_t;
#define DAP_INVALID_SOCKET INVALID_SOCKET
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int dapSocket_t;
#define DAP_INVALID_SOCKET -1
#endif

namespace {

cvar_t *dap_enable;
cvar_t *dap_address;
cvar_t *dap_port;
cvar_t *dap_verbose;
cvar_t *dap_allowRemote;
cvar_t *dap_token;

std::atomic<bool> g_running{ false };
std::atomic<bool> g_stopRequested{ false };
std::thread g_thread;
dapSocket_t g_serverSocket = DAP_INVALID_SOCKET;
dapSocket_t g_clientSocket = DAP_INVALID_SOCKET;

constexpr int kMaxDapContentLength = 64 * 1024;
constexpr size_t kMaxBufferedInput = 128 * 1024;

void CloseSocket( dapSocket_t &socket ) {
	if ( socket == DAP_INVALID_SOCKET ) {
		return;
	}
#ifdef _WIN32
	closesocket( socket );
#else
	shutdown( socket, SHUT_RDWR );
	close( socket );
#endif
	socket = DAP_INVALID_SOCKET;
}

bool SocketValid( dapSocket_t socket ) {
	return socket != DAP_INVALID_SOCKET;
}

std::string JsonStringValue( const char *json, const char *key ) {
	char pattern[64];
	const char *found;
	const char *value;
	const char *end;

	Com_sprintf( pattern, sizeof( pattern ), "\"%s\"", key );
	found = std::strstr( json ? json : "", pattern );
	if ( !found ) {
		return std::string();
	}
	found = std::strchr( found + std::strlen( pattern ), ':' );
	if ( !found ) {
		return std::string();
	}
	value = std::strchr( found, '"' );
	if ( !value ) {
		return std::string();
	}
	value++;
	end = std::strchr( value, '"' );
	if ( !end ) {
		return std::string();
	}
	return std::string( value, static_cast<size_t>( end - value ) );
}

int JsonIntValue( const char *json, const char *key, int fallback ) {
	char pattern[64];
	const char *found;

	Com_sprintf( pattern, sizeof( pattern ), "\"%s\"", key );
	found = std::strstr( json ? json : "", pattern );
	if ( !found ) {
		return fallback;
	}
	found = std::strchr( found + std::strlen( pattern ), ':' );
	if ( !found ) {
		return fallback;
	}
	return std::atoi( found + 1 );
}

bool IsLocalAddress( const char *address ) {
	return address && address[0] &&
		( std::strcmp( address, "127.0.0.1" ) == 0 ||
			std::strcmp( address, "localhost" ) == 0 ||
			std::strcmp( address, "::1" ) == 0 );
}

bool AddressAllowed( const char *address, int allowRemote, const char *token ) {
	if ( IsLocalAddress( address ) ) {
		return true;
	}
	return allowRemote && token && token[0];
}

bool RequestAuthorized( const char *request, const char *token ) {
	if ( !token || !token[0] ) {
		return true;
	}
	return JsonStringValue( request, "dapToken" ) == token;
}

void BuildAuthFailure( const char *request, char *out, int outSize ) {
	const int requestSeq = JsonIntValue( request, "seq", 0 );
	const std::string command = JsonStringValue( request, "command" );

	Com_sprintf( out, outSize,
		"{\"seq\":1,\"type\":\"response\",\"request_seq\":%d,\"success\":false,"
		"\"command\":\"%s\",\"message\":\"DAP authentication failed\"}",
		requestSeq, command.c_str() );
}

void BuildJsonResponse( const char *request, const char *token, char *out, int outSize ) {
	const int seq = 1;
	const int requestSeq = JsonIntValue( request, "seq", 0 );
	const std::string command = JsonStringValue( request, "command" );

	if ( !RequestAuthorized( request, token ) ) {
		BuildAuthFailure( request, out, outSize );
		return;
	}

	if ( command == "initialize" ) {
		Com_sprintf( out, outSize,
			"{\"seq\":%d,\"type\":\"response\",\"request_seq\":%d,\"success\":true,"
			"\"command\":\"initialize\",\"body\":{\"supportsConfigurationDoneRequest\":true,"
			"\"supportsEvaluateForHovers\":false,\"supportsSetVariable\":false}}" ,
			seq, requestSeq );
		return;
	}

	if ( command == "threads" ) {
		Com_sprintf( out, outSize,
			"{\"seq\":%d,\"type\":\"response\",\"request_seq\":%d,\"success\":true,"
			"\"command\":\"threads\",\"body\":{\"threads\":[{\"id\":1,\"name\":\"id Tech 3 main\"}]}}" ,
			seq, requestSeq );
		return;
	}

	if ( command == "disconnect" || command == "launch" || command == "attach" ||
			command == "configurationDone" ) {
		Com_sprintf( out, outSize,
			"{\"seq\":%d,\"type\":\"response\",\"request_seq\":%d,\"success\":true,"
			"\"command\":\"%s\"}",
			seq, requestSeq, command.c_str() );
		return;
	}

	if ( command.empty() ) {
		Com_sprintf( out, outSize,
			"{\"seq\":%d,\"type\":\"response\",\"request_seq\":%d,\"success\":false,"
			"\"command\":\"\",\"message\":\"Malformed DAP request\"}",
			seq, requestSeq );
		return;
	}

	Com_sprintf( out, outSize,
		"{\"seq\":%d,\"type\":\"response\",\"request_seq\":%d,\"success\":false,"
		"\"command\":\"%s\",\"message\":\"DAP command not implemented in native backend yet\"}",
		seq, requestSeq, command.c_str() );
}

bool SendAll( dapSocket_t socket, const char *data, size_t len ) {
	size_t sent = 0;
	while ( sent < len ) {
#ifdef _WIN32
		const int n = send( socket, data + sent, static_cast<int>( len - sent ), 0 );
#else
		const ssize_t n = send( socket, data + sent, len - sent, 0 );
#endif
		if ( n <= 0 ) {
			return false;
		}
		sent += static_cast<size_t>( n );
	}
	return true;
}

void HandleClient( dapSocket_t client ) {
	std::string input;
	char buffer[2048];

	while ( !g_stopRequested.load() ) {
#ifdef _WIN32
		const int bytesRead = recv( client, buffer, static_cast<int>( sizeof( buffer ) ), 0 );
#else
		const ssize_t bytesRead = recv( client, buffer, sizeof( buffer ), 0 );
#endif
		if ( bytesRead <= 0 ) {
			break;
		}
		input.append( buffer, static_cast<size_t>( bytesRead ) );

		for ( ;; ) {
			const size_t headerPos = input.find( "Content-Length:" );
			if ( headerPos == std::string::npos ) {
				if ( input.size() > 4096 ) {
					input.clear();
				}
				break;
			}
			const size_t headerEnd = input.find( "\r\n\r\n", headerPos );
			if ( headerEnd == std::string::npos ) {
				break;
			}
			const size_t lengthStart = headerPos + std::strlen( "Content-Length:" );
			const int contentLength = std::atoi( input.c_str() + lengthStart );
			const size_t bodyStart = headerEnd + 4;
			if ( contentLength <= 0 ) {
				input.erase( 0, bodyStart );
				continue;
			}
			if ( contentLength > kMaxDapContentLength ) {
				Com_Printf( S_COLOR_YELLOW "dap: rejected oversized message (%d bytes)\n", contentLength );
				return;
			}
			if ( bodyStart + static_cast<size_t>( contentLength ) > input.size() ) {
				if ( input.size() > kMaxBufferedInput ) {
					Com_Printf( S_COLOR_YELLOW "dap: dropped oversized partial message\n" );
					return;
				}
				break;
			}

			const std::string body = input.substr( bodyStart, static_cast<size_t>( contentLength ) );
			char json[4096];
			char packet[4608];
			BuildJsonResponse( body.c_str(), dap_token ? dap_token->string : "", json, sizeof( json ) );
			DAP_BuildProtocolMessage( json, packet, sizeof( packet ) );
			if ( dap_verbose && dap_verbose->integer ) {
				Com_Printf( "dap: %s -> %s\n", body.c_str(), json );
			}
			if ( !SendAll( client, packet, std::strlen( packet ) ) ) {
				return;
			}
			input.erase( 0, bodyStart + static_cast<size_t>( contentLength ) );
		}
	}
}

void ServerThread( std::string address, int port ) {
	sockaddr_in serverAddr;

#ifdef _WIN32
	WSADATA wsaData;
	if ( WSAStartup( MAKEWORD( 2, 2 ), &wsaData ) != 0 ) {
		Com_Printf( S_COLOR_YELLOW "dap: WSAStartup failed\n" );
		g_running = false;
		return;
	}
#endif

	g_serverSocket = socket( AF_INET, SOCK_STREAM, 0 );
	if ( !SocketValid( g_serverSocket ) ) {
		Com_Printf( S_COLOR_YELLOW "dap: failed to create socket\n" );
		g_running = false;
		return;
	}

	const int opt = 1;
#ifdef _WIN32
	setsockopt( g_serverSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>( &opt ), sizeof( opt ) );
#else
	setsockopt( g_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof( opt ) );
#endif

	std::memset( &serverAddr, 0, sizeof( serverAddr ) );
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons( static_cast<unsigned short>( port ) );
	if ( !AddressAllowed( address.empty() ? "127.0.0.1" : address.c_str(),
			dap_allowRemote ? dap_allowRemote->integer : 0,
			dap_token ? dap_token->string : "" ) ) {
		Com_Printf( S_COLOR_YELLOW
			"dap: refusing non-local bind '%s' without dap_allowRemote 1 and dap_token\n",
			address.empty() ? "0.0.0.0" : address.c_str() );
		g_running = false;
		return;
	}
	if ( address.empty() ) {
		serverAddr.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
	} else if ( inet_pton( AF_INET, address.c_str(), &serverAddr.sin_addr ) != 1 ) {
		Com_Printf( S_COLOR_YELLOW "dap: invalid address '%s'\n", address.c_str() );
		CloseSocket( g_serverSocket );
		g_running = false;
		return;
	}

	if ( bind( g_serverSocket, reinterpret_cast<sockaddr *>( &serverAddr ), sizeof( serverAddr ) ) < 0 ) {
		Com_Printf( S_COLOR_YELLOW "dap: failed to bind %s:%d\n", address.c_str(), port );
		CloseSocket( g_serverSocket );
		g_running = false;
		return;
	}
	if ( listen( g_serverSocket, 1 ) < 0 ) {
		Com_Printf( S_COLOR_YELLOW "dap: failed to listen\n" );
		CloseSocket( g_serverSocket );
		g_running = false;
		return;
	}

	Com_Printf( "dap: listening on %s:%d%s\n",
		address.empty() ? "127.0.0.1" : address.c_str(), port,
		dap_token && dap_token->string && dap_token->string[0] ? " (token required)" : "" );
	while ( !g_stopRequested.load() ) {
		sockaddr_in clientAddr;
#ifdef _WIN32
		int clientAddrLen = sizeof( clientAddr );
		g_clientSocket = accept( g_serverSocket, reinterpret_cast<sockaddr *>( &clientAddr ), &clientAddrLen );
#else
		socklen_t clientAddrLen = sizeof( clientAddr );
		g_clientSocket = accept( g_serverSocket, reinterpret_cast<sockaddr *>( &clientAddr ), &clientAddrLen );
#endif
		if ( !SocketValid( g_clientSocket ) ) {
			if ( !g_stopRequested.load() ) {
				Com_Printf( S_COLOR_YELLOW "dap: accept failed\n" );
			}
			continue;
		}
		Com_Printf( "dap: client connected from %s\n", inet_ntoa( clientAddr.sin_addr ) );
		HandleClient( g_clientSocket );
		CloseSocket( g_clientSocket );
		Com_Printf( "dap: client disconnected\n" );
	}
	CloseSocket( g_serverSocket );
#ifdef _WIN32
	WSACleanup();
#endif
	g_running = false;
}

void DAP_Start_f( void ) {
	if ( g_running.load() ) {
		Com_Printf( "dap: already running\n" );
		return;
	}
	g_stopRequested = false;
	g_running = true;
	g_thread = std::thread( ServerThread,
		dap_address && dap_address->string ? dap_address->string : "127.0.0.1",
		dap_port ? dap_port->integer : 9229 );
}

void DAP_Stop_f( void ) {
	g_stopRequested = true;
	CloseSocket( g_clientSocket );
	CloseSocket( g_serverSocket );
	if ( g_thread.joinable() ) {
		g_thread.join();
	}
	g_running = false;
	Com_Printf( "dap: stopped\n" );
}

void DAP_Status_f( void ) {
	Com_Printf( "dap: %s address=%s port=%d backend=native-stub\n",
		g_running.load() ? "running" : "stopped",
		dap_address && dap_address->string ? dap_address->string : "127.0.0.1",
		dap_port ? dap_port->integer : 9229 );
}

}  // namespace

extern "C" {

void DAP_Init( void ) {
	dap_enable = Cvar_Get( "dap_enable", "0", CVAR_ARCHIVE );
	dap_address = Cvar_Get( "dap_address", "127.0.0.1", CVAR_ARCHIVE );
	dap_port = Cvar_Get( "dap_port", "9229", CVAR_ARCHIVE );
	dap_verbose = Cvar_Get( "dap_verbose", "0", CVAR_ARCHIVE );
	dap_allowRemote = Cvar_Get( "dap_allowRemote", "0", CVAR_ARCHIVE );
	dap_token = Cvar_Get( "dap_token", "", CVAR_TEMP );

	Cmd_AddCommand( "dap_start", DAP_Start_f );
	Cmd_AddCommand( "dap_stop", DAP_Stop_f );
	Cmd_AddCommand( "dap_status", DAP_Status_f );

	if ( dap_enable->integer ) {
		DAP_Start_f();
	}
}

void DAP_Shutdown( void ) {
	DAP_Stop_f();
	Cmd_RemoveCommand( "dap_start" );
	Cmd_RemoveCommand( "dap_stop" );
	Cmd_RemoveCommand( "dap_status" );
}

void DAP_Frame( void ) {
}

int DAP_IsRunning( void ) {
	return g_running.load() ? 1 : 0;
}

int DAP_BuildProtocolMessage( const char *json, char *out, int outSize ) {
	const int jsonLen = json ? static_cast<int>( std::strlen( json ) ) : 0;
	if ( !json || !out || outSize <= 0 ) {
		return 0;
	}
	if ( jsonLen + 32 >= outSize ) {
		out[0] = '\0';
		return 0;
	}
	Com_sprintf( out, outSize, "Content-Length: %d\r\n\r\n%s", jsonLen, json );
	return 1;
}

int DAP_HandleJsonForTest( const char *json, char *out, int outSize ) {
	if ( !json || !out || outSize <= 0 ) {
		return 0;
	}
	BuildJsonResponse( json, "", out, outSize );
	return out[0] != '\0' ? 1 : 0;
}

int DAP_HandleJsonWithTokenForTest( const char *json, const char *token, char *out, int outSize ) {
	if ( !json || !out || outSize <= 0 ) {
		return 0;
	}
	BuildJsonResponse( json, token ? token : "", out, outSize );
	return out[0] != '\0' ? 1 : 0;
}

int DAP_IsAddressAllowedForTest( const char *address, int allowRemote, const char *token ) {
	return AddressAllowed( address, allowRemote, token ) ? 1 : 0;
}

}
