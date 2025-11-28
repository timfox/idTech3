# Networking Improvements

This document describes the modern networking enhancements added to the id Tech 3 engine.

## Overview

The enhanced networking system adds HTTP/2 support, connection pooling, rate limiting, improved IPv6 handling, and network statistics tracking to improve download performance and reliability.

## Features Implemented

### 1. HTTP/2 Support

**Status:** ✅ Implemented

HTTP/2 support is automatically enabled when:
- curl library supports HTTP/2 (version 7.33.0+)
- `cl_http2_enable` cvar is set to 1 (default: 1)

**CVars:**
- `cl_http2_enable` - Enable HTTP/2 support (default: 1)
- `cl_http2_prefer` - Prefer HTTP/2 over HTTP/1.1 when both available (default: 1)

**Benefits:**
- Multiplexing: Multiple requests over a single connection
- Header compression: Reduced overhead
- Server push: Potential for faster content delivery
- Better performance on high-latency connections

### 2. Connection Pooling

**Status:** ✅ Implemented

Connection pooling reuses HTTP connections to reduce overhead and improve performance.

**CVars:**
- `cl_connection_pool_enable` - Enable connection pooling (default: 1)
- `cl_connection_pool_max` - Maximum pooled connections (default: 10, range: 1-50)
- `cl_connection_pool_timeout` - Idle connection timeout in ms (default: 30000, range: 1000-300000)

**How it works:**
- Connections are reused for requests to the same hostname/port
- Idle connections are kept alive for faster subsequent requests
- Old idle connections are automatically cleaned up
- Limits prevent excessive connection usage

**Benefits:**
- Reduced connection establishment overhead
- Faster downloads from the same server
- Better resource utilization

### 3. Rate Limiting

**Status:** ✅ Implemented

Rate limiting prevents overwhelming servers and helps avoid being blocked.

**CVars:**
- `cl_rate_limit_enable` - Enable rate limiting (default: 1)
- `cl_rate_limit_max_per_sec` - Maximum requests per second (default: 10, range: 1-100)
- `cl_rate_limit_max_concurrent` - Maximum concurrent requests (default: 5, range: 1-20)

**How it works:**
- Tracks requests per second in a sliding window
- Limits concurrent active requests
- Automatically throttles when limits are reached

**Benefits:**
- Prevents server overload
- Reduces risk of being blocked
- More polite network behavior

### 4. IPv6 Improvements

**Status:** ✅ Implemented

Enhanced IPv6 support with better dual-stack handling and preference settings.

**CVars:**
- `cl_prefer_ipv6` - Prefer IPv6 connections over IPv4 (default: 0)

**Features:**
- Automatic IPv6 detection and support
- Configurable IPv6 preference
- Dual-stack support (IPv4 and IPv6)
- Better connection handling for IPv6 addresses

**Benefits:**
- Future-proof networking
- Better connectivity in IPv6-only environments
- Improved performance on IPv6 networks

### 5. Network Statistics

**Status:** ✅ Implemented

Comprehensive network statistics tracking for monitoring and debugging.

**CVar:**
- `cl_net_stats` - Show network statistics (default: 0)

**Tracked Metrics:**
- Total bytes sent/received
- Total requests (success/failed)
- Average response time
- HTTP/2 vs HTTP/1.1 usage
- IPv6 vs IPv4 usage

**Benefits:**
- Performance monitoring
- Debugging network issues
- Understanding connection patterns

### 6. WebSocket Support

**Status:** ⏳ Placeholder (Future Implementation)

WebSocket support is planned but requires additional library integration (libwebsockets).

**Planned Features:**
- Real-time bidirectional communication
- Lower latency than HTTP polling
- Better for real-time game features

## Usage

### Automatic Usage

The enhanced networking is automatically used when:
1. curl is enabled (`USE_CURL` is defined)
2. Enhanced networking is initialized (happens automatically on first download)

### Manual Configuration

Users can configure behavior via CVars:

```
// Enable HTTP/2
set cl_http2_enable 1
set cl_http2_prefer 1

// Configure connection pooling
set cl_connection_pool_enable 1
set cl_connection_pool_max 10
set cl_connection_pool_timeout 30000

// Configure rate limiting
set cl_rate_limit_enable 1
set cl_rate_limit_max_per_sec 10
set cl_rate_limit_max_concurrent 5

// Prefer IPv6
set cl_prefer_ipv6 1

// Show statistics
set cl_net_stats 1
```

## Technical Details

### Architecture

The enhanced networking system consists of:

1. **cl_net_enhanced.h** - Header file with type definitions and function prototypes
2. **cl_net_enhanced.c** - Implementation of enhanced features
3. **Integration** - Seamless integration with existing `cl_curl.c` code

### Compatibility

- **Backward Compatible:** Falls back to standard curl behavior if enhanced features fail
- **Graceful Degradation:** Works even if HTTP/2 is not supported
- **No Breaking Changes:** Existing code continues to work

### Dependencies

- curl library (already required)
- curl version 7.33.0+ for HTTP/2 support (optional, gracefully degrades)

## Performance Impact

### Expected Improvements

- **Connection Pooling:** 20-50% faster for repeated downloads from same server
- **HTTP/2:** 10-30% faster on high-latency connections
- **Rate Limiting:** Prevents server overload, may slightly slow aggressive downloads

### Overhead

- Minimal memory overhead (~few KB per pooled connection)
- Negligible CPU overhead
- Network statistics add minimal tracking overhead

## Future Enhancements

1. **WebSocket Implementation** - Add libwebsockets integration
2. **HTTP/3 Support** - When curl adds HTTP/3 support
3. **Advanced Statistics** - More detailed metrics and visualization
4. **Adaptive Rate Limiting** - Dynamic rate limiting based on server response
5. **Connection Health Monitoring** - Detect and replace bad connections

## Troubleshooting

### HTTP/2 Not Working

- Check curl version: `curl --version` should show HTTP2
- Verify `cl_http2_enable` is set to 1
- Check server supports HTTP/2

### Connection Pool Issues

- Reduce `cl_connection_pool_max` if experiencing memory issues
- Increase `cl_connection_pool_timeout` if connections timeout too quickly

### Rate Limiting Too Aggressive

- Increase `cl_rate_limit_max_per_sec` if downloads are too slow
- Increase `cl_rate_limit_max_concurrent` for parallel downloads

## Code Examples

### Using Enhanced Download

```c
// Automatic usage - enhanced features are used automatically
CL_cURL_BeginDownload(localName, remoteURL);

// Or use enhanced API directly
NET_Enhanced_BeginDownload(localName, remoteURL, use_http2, prefer_ipv6);
```

### Checking Statistics

```c
net_stats_t *stats = NET_Stats_Get();
Com_Printf("Total requests: %llu\n", stats->requests_total);
Com_Printf("HTTP/2 requests: %d\n", stats->http2_requests);
Com_Printf("Average response time: %d ms\n", stats->average_response_time);
```

## References

- [curl HTTP/2 Documentation](https://curl.se/docs/http2.html)
- [HTTP/2 Specification](https://httpwg.org/specs/rfc7540.html)
- [IPv6 Best Practices](https://www.ietf.org/rfc/rfc6724.txt)

