# DDoS Protection and Rate Limiting

This document outlines the DDoS protection measures implemented in the id Tech 3 engine to prevent network-based attacks and ensure server stability.

## Overview

The engine includes multiple layers of protection against Distributed Denial of Service (DDoS) attacks, including rate limiting, packet validation, and connection management features.

## Features Implemented

### 1. Connection Rate Limiting

**Status:** ✅ Implemented

Connection rate limiting prevents excessive connection attempts from individual IP addresses.

**CVars:**
- `sv_maxRate` - Maximum bytes per second per client (default: 25000, range: 0-90000)
- `sv_maxRateForAll` - Maximum bytes per second for all clients combined (default: 0, disabled)
- `sv_minRate` - Minimum rate limit for clients (default: 0)

**Implementation:**
- Per-client rate limiting based on IP address
- Automatic rate reduction for high-traffic clients
- Configurable limits prevent bandwidth exhaustion

### 2. Packet Flood Protection

**Status:** ✅ Implemented

Packet flood protection prevents excessive packet sending that could overwhelm the server.

**Features:**
- Packet rate monitoring per client
- Automatic client dropping for excessive packet rates
- Configurable thresholds for different packet types

**CVars:**
- `sv_packetFloodProtect` - Enable packet flood protection (default: 1)
- `sv_packetFloodThreshold` - Maximum packets per second (default: 1000)
- `sv_packetFloodDrop` - Drop clients exceeding threshold (default: 1)

### 3. Malformed Packet Filtering

**Status:** ✅ Implemented

Invalid and malformed network packets are filtered and logged to prevent exploitation.

**Protection Against:**
- Invalid packet headers
- Oversized packets
- Malformed game state data
- Corrupted entity updates

**Implementation:**
- Packet validation at network layer
- Size limits on all packet types
- Checksum validation for critical packets
- Logging of suspicious packet patterns

### 4. Connection Timeouts

**Status:** ✅ Implemented

Automatic disconnection of inactive or slow clients prevents resource exhaustion.

**CVars:**
- `sv_timeout` - Client timeout in seconds (default: 120)
- `sv_zombieTime` - Time before dropping zombie clients (default: 2)
- `sv_reconnectLimit` - Maximum reconnection attempts (default: 3)

### 5. IP Address Filtering

**Status:** ✅ Implemented

IP-based filtering allows blocking problematic clients or networks.

**Features:**
- IP ban lists with CIDR support
- Temporary bans with automatic expiration
- Whitelist support for trusted networks
- Ban reason logging

**Commands:**
- `addIP <ip/mask>` - Add IP to ban list
- `removeIP <ip/mask>` - Remove IP from ban list
- `listIP` - Show current IP filters
- `flushIP` - Clear all IP filters

### 6. Challenge-Response System

**Status:** ✅ Implemented

Challenge-response authentication prevents unauthorized connections and reduces spoofing attacks.

**Features:**
- Cryptographic challenges for connection validation
- Anti-spoofing measures using client identification
- Rate-limited challenge generation
- Timeout protection for pending challenges

### 7. Bandwidth Throttling

**Status:** ✅ Implemented via SDL_net

Bandwidth throttling prevents individual clients from consuming excessive server resources.

**Implementation:**
- Per-client bandwidth tracking
- Automatic throttling of high-bandwidth clients
- Fair bandwidth distribution across all clients
- Configurable bandwidth limits

### 8. Server Query Protection

**Status:** ✅ Implemented

Server query (heartbeat/status) protection prevents query floods.

**Features:**
- Query rate limiting per IP
- Query result caching
- Minimal information disclosure
- Configurable query timeouts

**CVars:**
- `sv_queryRateLimit` - Maximum queries per second per IP (default: 10)
- `sv_queryCacheTime` - Query result cache time in seconds (default: 30)

## Configuration Examples

### Basic DDoS Protection

```c
// Enable all protection features
set sv_packetFloodProtect "1"
set sv_timeout "120"
set sv_maxRate "25000"

// Rate limiting
set sv_queryRateLimit "10"
set sv_packetFloodThreshold "1000"
```

### High-Security Configuration

```c
// Strict rate limiting
set sv_maxRate "10000"
set sv_minRate "1000"
set sv_packetFloodThreshold "500"

// Aggressive timeouts
set sv_timeout "60"
set sv_zombieTime "1"

// Enable all filtering
set sv_packetFloodProtect "1"
set sv_queryRateLimit "5"
```

### Tournament/Competitive Settings

```c
// Relaxed for legitimate high-traffic scenarios
set sv_maxRate "50000"
set sv_packetFloodThreshold "2000"
set sv_timeout "300"

// Maintain basic protection
set sv_packetFloodProtect "1"
set sv_queryRateLimit "20"
```

## Monitoring and Logging

### Log Analysis

The server logs DDoS-related events for monitoring:

```
Client 192.168.1.100 dropped: rate exceeded (15000 > 10000)
Packet flood detected from 10.0.0.1: 1200 packets/sec
IP 203.0.113.1 banned: excessive connection attempts
```

### Monitoring Commands

- `status` - Show connected clients and rates
- `dumpuser <client>` - Show detailed client information
- `net_stats` - Display network statistics

## Performance Impact

- **Memory**: Minimal (~1KB per client for tracking)
- **CPU**: Low overhead for rate limiting checks
- **Network**: No impact on legitimate traffic

## Best Practices

1. **Regular Monitoring**: Check logs for suspicious activity
2. **Rate Tuning**: Adjust rates based on server capacity and typical usage
3. **IP Filtering**: Use for persistent attackers
4. **Update Regularly**: Keep server software updated for latest protections
5. **Firewall Integration**: Use DDoS protection in conjunction with network firewalls

## Troubleshooting

### False Positives

If legitimate clients are being dropped:
- Increase `sv_maxRate` for high-bandwidth clients
- Adjust `sv_packetFloodThreshold` for high-packet-rate scenarios
- Use `sv_minRate` to set floor limits

### Performance Issues

If protection causes lag:
- Reduce checking frequency for low-risk environments
- Use `sv_maxRateForAll` instead of per-client limits
- Disable non-essential protections for local networks

### Configuration Conflicts

If settings conflict:
- `sv_maxRate` takes precedence over client-specific rates
- IP filters override rate limits for banned addresses
- Challenge-response is always active regardless of other settings

## Integration with External Tools

### Firewall Rules

```bash
# Example iptables rules for additional protection
iptables -A INPUT -p udp --dport 27960 -m limit --limit 10/sec -j ACCEPT
iptables -A INPUT -p udp --dport 27960 -j DROP
```

### Monitoring Scripts

```bash
# Monitor for DDoS patterns
tail -f ~/.q3a/baseq3/qconsole.log | grep -E "(dropped|exceeded|banned)"
```

## Security Considerations

1. **Default Settings**: Conservative defaults balance security and usability
2. **Rate Limiting**: Prevents bandwidth exhaustion attacks
3. **Packet Validation**: Prevents buffer overflow exploits
4. **IP Filtering**: Blocks known malicious actors
5. **Logging**: Enables incident response and forensic analysis

## Future Enhancements

- [ ] GeoIP-based filtering
- [ ] Machine learning anomaly detection
- [ ] Integration with external DDoS protection services
- [ ] Advanced traffic analysis and reporting

## References

- Network protocol documentation
- SDL_net library documentation
- RFC specifications for UDP-based protocols