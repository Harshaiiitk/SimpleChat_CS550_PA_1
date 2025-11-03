# DSDV Routing and NAT Traversal Implementation

## Overview
This enhanced version of SimpleChat implements:
1. **Destination-Sequenced Distance-Vector (DSDV) Routing** for efficient point-to-point messaging
2. **NAT Traversal** capabilities using route rumor propagation and public endpoint discovery

## Part 1: DSDV Routing Implementation

### 1.1 Routing Table Structure
Each node maintains a routing table with entries containing:
- **Next Hop**: IP address and port of the next node to reach destination
- **Sequence Number**: DSDV sequence for freshness comparison  
- **Hop Count**: Number of hops to reach destination
- **Direct Flag**: Whether this is a direct route
- **Public Endpoint**: Discovered public IP/port for NAT traversal

### 1.2 Route Updates
Routes are updated when:
1. **Receiving any Rumor message**: If message.SeqNo > currentSeqNo[origin], update route
2. **Route rumors**: Periodic announcements every 60 seconds
3. **Message receipts**: Learn routes from incoming messages

Update logic:
```cpp
if (newRoute.sequenceNumber > oldRoute.sequenceNumber) {
    // Newer route - always accept
} else if (newRoute.sequenceNumber == oldRoute.sequenceNumber && newRoute.isDirect) {
    // Same seq but direct route (prefer for NAT traversal)
} else if (newRoute.sequenceNumber == oldRoute.sequenceNumber && 
           newRoute.hopCount < oldRoute.hopCount) {
    // Same seq but shorter path
}
```

### 1.3 Private Messaging
Private messages use DSDV routing with:
- **Destination field**: Target node ID
- **Hop Limit**: Prevents infinite forwarding loops (default: 10)
- **Routing decision**: Check routing table, forward to next hop

Message format:
```json
{
    "Type": "private",
    "Dest": "NodeB",
    "Origin": "NodeA",
    "ChatText": "Hello",
    "HopLimit": 10,
    "LastIP": "sender's IP",
    "LastPort": sender's port
}
```

## Part 2: NAT Traversal

### 2.1 Rendezvous Server (-noforward mode)
The rendezvous server facilitates NAT traversal:
- Accepts connections from NATted clients
- Forwards route rumors but NOT chat messages
- Helps nodes discover each other's public endpoints

Usage:
```bash
./SimpleChat --client NodeS --port 45678 --noforward
```

### 2.2 NAT Hole-Punching
Nodes behind NAT can communicate by:
1. **Connecting to rendezvous server**: Establishes initial connectivity
2. **Route rumor exchange**: Discovers other nodes and their public endpoints
3. **Direct communication**: Uses discovered public endpoints

All messages include NAT traversal fields:
```json
{
    "LastIP": "10.1.1.1",     // Sender's local IP
    "LastPort": 11111          // Sender's local port
}
```

When a node receives a message, it compares:
- **LastIP/LastPort**: What sender thinks its address is
- **Observed address**: What we actually see

If different, NAT is detected and public endpoint is stored.

### 2.3 Route Preference
Routes are preferred based on:
1. **Sequence number** (higher = fresher)
2. **Directness** (direct routes preferred for NAT traversal)
3. **Hop count** (shorter = better)

## Testing Instructions

### Basic DSDV Testing (Local)
```bash
# Build the project
./build.sh

# Launch 4-node network
./launch_ring.sh

# In the GUI:
# 1. Wait for nodes to appear in "Available Nodes" list
# 2. Double-click a node to send private message
# 3. Observe routing decisions in chat log
```

### NAT Traversal Testing

#### Option 1: Using Network Namespaces (Linux)
```bash
# Run as root
sudo ./nat_test.sh

# This creates:
# - Rendezvous server on host (port 45678)
# - NodeN1 in NAT namespace 1 (10.1.1.1:11111)
# - NodeN2 in NAT namespace 2 (10.2.2.1:22222)
```

#### Option 2: Manual Setup
```bash
# Terminal 1 - Rendezvous server
./build/bin/SimpleChat --client NodeS --port 45678 --noforward

# Terminal 2 - NAT Client 1
./build/bin/SimpleChat --client NodeN1 --port 11111 --connect 45678

# Terminal 3 - NAT Client 2  
./build/bin/SimpleChat --client NodeN2 --port 22222 --connect 45678
```

### Verification Steps

#### DSDV Routing:
1. **Route Discovery**: Nodes should appear in "Available Nodes" list within 60s
2. **Sequence Numbers**: Each route shows (seq:N) indicating DSDV sequence
3. **Private Messages**: Should route through intermediate nodes when no direct path
4. **Route Updates**: New routes accepted when sequence number increases

#### NAT Traversal:
1. **Public Endpoint Discovery**: Log shows "NAT detected for NodeX"
2. **Direct Communication**: NodeN1 and NodeN2 can message despite NAT
3. **No-Forward Mode**: Rendezvous server doesn't forward chat messages
4. **Route Propagation**: Route rumors still forwarded by rendezvous

## Command-Line Options

```bash
./SimpleChat [OPTIONS]

Options:
  -c, --client <ID>        Client identifier (default: Client1)
  -p, --port <PORT>        UDP port to bind (default: 9001)
  -P, --peer <IP:PORT>     Initial peer for discovery (repeatable)
  -n, --noforward          No-forward mode (rendezvous server)
  -C, --connect <PORT>     Connect to rendezvous at localhost:PORT

Examples:
  # Basic node
  ./SimpleChat --client Node1 --port 9001
  
  # With initial peer
  ./SimpleChat --client Node2 --port 9002 --peer 127.0.0.1:9001
  
  # Rendezvous server
  ./SimpleChat --client Server --port 45678 --noforward
  
  # NAT client
  ./SimpleChat --client NATNode --port 11111 --connect 45678
```

## GUI Features

### Main Window
- **Chat Log**: Shows all messages and routing decisions
- **Available Nodes**: List of discovered nodes with DSDV info
  - Format: `NodeID (seq:N, hop:H) [D] [NAT]`
  - `[D]` = Direct route available
  - `[NAT]` = Public endpoint known
- **Message Input**: Enter messages to send
- **Destination Selector**: Choose target for direct messages
- **Buttons**:
  - **Send**: Send to selected destination
  - **Broadcast**: Send to all nodes
  - **Private Msg**: Send private message using DSDV routing

### Node List Interaction
- **Double-click**: Send private message to selected node
- **Visual indicators**:
  - Sequence number shows route freshness
  - Hop count shows distance
  - [D] indicates direct connectivity
  - [NAT] indicates NAT traversal possible

## Architecture Details

### Message Types
1. **message**: Regular P2P/broadcast messages
2. **private**: Private messages with DSDV routing
3. **route_rumor**: DSDV route announcements
4. **discovery**: Peer discovery
5. **ack**: Message acknowledgments
6. **vector_clock**: Anti-entropy sync
7. **sync_message**: Missing message sync

### Routing Table Management
- Updated on receipt of any message with origin
- Periodic route rumors ensure eventual consistency
- Stale routes timeout after 30 seconds
- Better routes replace existing based on DSDV rules

### NAT Traversal Strategy
1. **Endpoint Discovery**: Learn public addresses from packet headers
2. **Route Preference**: Prefer direct routes when available
3. **Hole Punching**: Use discovered endpoints for direct communication
4. **Fallback**: Route through intermediaries if direct fails

## Troubleshooting

### Common Issues

#### Routes not appearing
- Wait up to 60 seconds for route rumors
- Check nodes are connected (peer discovery working)
- Verify no firewall blocking UDP

#### NAT traversal not working
- Ensure rendezvous server is in --noforward mode
- Check NAT rules if using namespaces
- Verify nodes connect to rendezvous first

#### Private messages not delivered
- Check routing table has path to destination
- Verify hop limit not exceeded (default: 10)
- Look for "no route" messages in log

### Debug Output
The chat log shows detailed routing information:
- "New route to X via Y:Z" - Route discovered
- "Updated route to X" - Better route found
- "Routing via X:Y" - Message forwarding decision
- "NAT detected for X" - Public endpoint discovered
- "Forwarding private message" - Intermediate routing

## Known Limitations

1. **Route Convergence**: Takes up to 60 seconds for full route discovery
2. **NAT Types**: Works best with cone NAT, limited with symmetric NAT
3. **Scalability**: Routing table grows with network size
4. **Security**: No encryption or authentication implemented
5. **Message Ordering**: No guaranteed ordering for private messages

## Implementation Files

- **simplechatp2p.h**: Enhanced header with DSDV structures
- **simplechatp2p.cpp**: Main implementation with routing logic
- **main.cpp**: Added --noforward and --connect options
- **nat_test.sh**: Network namespace setup for NAT testing
- **launch_ring.sh**: Updated launch script with DSDV info

## References

- DSDV: Perkins & Bhagwat, "Highly Dynamic Destination-Sequenced Distance-Vector Routing"
- NAT Traversal: RFC 3489 (STUN), RFC 5245 (ICE concepts)
- Qt Network: https://doc.qt.io/qt-6/qtnetwork-index.html