# SimpleChat P2P - UDP-based Messaging Application with DSDV Routing and NAT Traversal

A Qt6-based distributed chat application that implements a peer-to-peer (P2P) messaging system over UDP with **Destination-Sequenced Distance-Vector (DSDV) routing**, **NAT traversal**, and anti-entropy synchronization. This implementation includes route rumor propagation, private messaging with hop limits, and rendezvous server support.

## Architecture Overview

### P2P Network Model with DSDV Routing
- Each client listens on a UDP port and discovers peers via periodic local port discovery and optional manual peer addition.
- **DSDV Routing**: Maintains a routing table with sequence numbers, preferring fresher routes and direct connections.
- **Route Rumors**: Periodically broadcast route announcements (every 60s) to propagate routing information.
- **Private Messaging**: Point-to-point messages routed through intermediate nodes using DSDV routing table.
- **NAT Traversal**: Automatically discovers public endpoints via route rumors and prefers direct connections when available.
- **Anti-Entropy**: Uses vector-clock-like summaries so peers exchange missing messages across multiple hops.

### Key Concepts Implemented

1. **UDP Messaging**: Uses Qt's QUdpSocket for peer-to-peer and broadcast communication
2. **DSDV Routing**: Destination-Sequenced Distance-Vector routing with sequence numbers and hop counts
3. **Route Rumors**: Periodic route announcements (`route_rumor` messages) for topology discovery
4. **NAT Traversal**: Public endpoint discovery via `LastIP`/`LastPort` fields; route preference for direct connections
5. **Private Messaging**: Hop-limited private messages (`Dest`, `HopLimit`) routed via DSDV table
6. **Rendezvous Server**: No-forward mode (`--noforward`) for nodes that forward route rumors but not chat messages
7. **Message Serialization**: Messages are serialized using Qt's QDataStream and QVariantMap
8. **Peer Discovery**: Periodic local port discovery and manual IP:Port addition
9. **Sequence Numbering**: Per-origin sequence numbers for ordering and vector clock summarization
10. **Anti-Entropy Sync**: Periodic vector clock exchange to identify and send missing messages
11. **Retransmission Timers**: Unacknowledged messages are resent after short intervals

## Features

- **GUI Interface**: Qt6 UI with chat log, destination selector, node list, and broadcast
- **DSDV Routing**: Automatic routing table management with sequence numbers and hop counts
- **Route Rumors**: Periodic route announcements (60s interval) for topology discovery
- **Private Messaging**: Point-to-point messages with hop limits, routed via DSDV table
- **NAT Traversal**: Automatic public endpoint discovery and preference for direct routes
- **Rendezvous Server Mode**: `--noforward` flag to run as rendezvous server (forwards route rumors only)
- **UDP P2P & Broadcast**: Direct peer messaging; broadcast with Destination = "-1"
- **Discovery**: Automatic local port scan (9000-9009) and manual Add Peer (IP:Port)
- **Anti-Entropy**: Vector clock exchange and on-demand sync of missing messages
- **Reliability on UDP**: Acks and retransmissions (2s) for improved delivery
- **Message Ordering**: Per-origin sequence numbers maintained and summarized

## Message Format

All messages are QVariantMap-serialized via QDataStream with a magic header (0xCAFEBABE) and size prefix.

### Chat Messages
```cpp
{
    "Type": "message",
    "ChatText": "The actual message content",
    "Origin": "Unique sender ID",
    "Destination": "Peer ID or \"-1\" for broadcast",
    "Sequence": <int>,
    "Timestamp": <ms since epoch>,
    "LastIP": "<sender_ip>",      // NAT traversal field
    "LastPort": <sender_port>      // NAT traversal field
}
```

### Private Messages (DSDV Routed)
```cpp
{
    "Type": "private",
    "Origin": "Sender ID",
    "Dest": "Destination node ID",
    "ChatText": "Message content",
    "HopLimit": <quint32>,         // Default: 10, decremented on each forward
    "LastIP": "<sender_ip>",       // NAT traversal field
    "LastPort": <sender_port>      // NAT traversal field
}
```

### Route Rumors (DSDV)
```cpp
{
    "Type": "route_rumor",
    "Origin": "Node ID",
    "SeqNo": <int>,                // DSDV sequence number (incremented per announcement)
    "LastIP": "<sender_ip>",       // NAT traversal field
    "LastPort": <sender_port>      // NAT traversal field
}
```

### Ack Messages
```cpp
{
    "Type": "ack",
    "Origin": "Ack sender ID",
    "AckOrigin": "Original message origin",
    "AckSequence": <int>
}
```

### Discovery Messages
```cpp
{ 
    "Type": "discovery", 
    "Origin": "<id>", 
    "Port": <int>,
    "LastIP": "<sender_ip>",
    "LastPort": <sender_port>
}
{ 
    "Type": "discovery_response", 
    "Origin": "<id>", 
    "Port": <int>,
    "LastIP": "<sender_ip>",
    "LastPort": <sender_port>
}
```

### Anti-Entropy Messages
```cpp
{ 
    "Type": "vector_clock", 
    "Origin": "<id>", 
    "VectorClock": { "<origin>": <maxSeq>, ... } 
}
{ 
    "Type": "sync_message", 
    "Origin": "<id>", 
    "SyncOrigin": "<origin>", 
    "SyncSequence": <int>, 
    "SyncDestination": "<id|-1>", 
    "SyncText": "..." 
}
```

## Build Requirements

- **Qt6**: Core, Widgets, and Network modules
- **CMake**: Version 3.16 or higher  
- **C++17**: Compatible compiler (GCC, Clang, MSVC)
- **Git**: For version control

### Ubuntu/Debian Installation
```bash
sudo apt update
sudo apt install qt6-base-dev qt6-base-dev-tools cmake build-essential git
```

### macOS Installation
```bash
brew install qt6 cmake git
```

**Note**: Linux network namespaces (for NAT simulation) are not available on macOS. Run NAT tests on Linux or in a Linux VM.

### Windows Installation
- Install Qt6 from [official website](https://www.qt.io/download)
- Install CMake from [cmake.org](https://cmake.org/download/)
- Use Visual Studio or MinGW for compilation

### Linux Requirements for NAT Testing
For NAT traversal testing, you'll need:
- `iproute2` (provides `ip` command)
- `iptables` (for NAT rules)
```bash
sudo apt install iproute2 iptables  # Ubuntu/Debian
```

## Building the Project

1. **Clone the repository** (or extract the provided files):
   ```bash
   git clone <repository-url>
   cd simplechat
   ```

2. **Make scripts executable**:
   ```bash
   chmod +x build.sh
   chmod +x launch_ring.sh
   chmod +x tests/*.sh
   ```

3. **Build the project**:
   ```bash
   ./build.sh
   ```

### Manual Build Process
If the build script doesn't work:
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Running the Application

### Automatic Launch (Recommended)
```bash
./launch_ring.sh
```
This launches 4 clients using UDP P2P. Each instance listens on its own port and performs local discovery. Some are primed with a known peer for faster discovery.

### Manual Launch
Launch each client in separate terminals:

```bash
# Terminal 1
./build/bin/SimpleChat --client Client1 --port 9001

# Terminal 2
./build/bin/SimpleChat --client Client2 --port 9002 --peer 127.0.0.1:9001

# Terminal 3
./build/bin/SimpleChat --client Client3 --port 9003 --peer 127.0.0.1:9001

# Terminal 4
./build/bin/SimpleChat --client Client4 --port 9004 --peer 127.0.0.1:9001
```

### Rendezvous Server Mode
Run a node in rendezvous mode (forwards route rumors but not chat messages):
```bash
./build/bin/SimpleChat --client Rendezvous --port 45678 --noforward
```

### NAT Traversal Testing
Connect to a rendezvous server from a NAT environment:
```bash
# On NATed client (Linux network namespace)
./build/bin/SimpleChat --client NodeN1 --port 11111 --connect 45678
```

## Usage Instructions

1. **Start the P2P network** using `./launch_ring.sh` or manual launch as above
2. **Wait for discovery**: The status shows connected peers and the node list populates with DSDV routes
3. **Select destination**: Use the dropdown to choose a peer or select Broadcast
4. **Send messages**: Type a message and click "Send" or press Enter
5. **Private messaging**: Double-click a node in the node list to send a private message, or use "Private Msg" button
6. **Route discovery**: Route rumors propagate every 60s; routing table updates automatically
7. **Anti-entropy**: Peers exchange vector clocks periodically; missing messages are synced automatically

### DSDV Routing & Private Messaging

- **Route Rumors**: Sent automatically every 60 seconds and at startup
  - Format: `{"Type":"route_rumor","Origin":"<NodeID>","SeqNo":N}`
  - Forwarded to random neighbors to propagate routing information
  - Routing table updates: prefer higher sequence numbers; if same, prefer direct routes; if same, prefer fewer hops

- **Private Messages**: 
  - Format: `{"Type":"private","Origin":"A","Dest":"B","ChatText":"...","HopLimit":10}`
  - Routed via DSDV routing table using `Dest` field
  - Hop limit decremented on each forward; message dropped if limit reaches 0
  - If no route found, message is broadcast to discover route

- **GUI Node List**: Shows discovered nodes with sequence numbers, hop counts, and NAT indicators
  - Double-click a node to send a private message
  - Nodes marked with `[D]` are direct routes
  - Nodes marked with `[NAT]` have discovered public endpoints

### NAT Traversal & Rendezvous Mode

- **Public Endpoint Discovery**: All messages include `LastIP` and `LastPort` fields
  - Receivers observe the sender's public endpoint (what they see)
  - If `LastIP`/`LastPort` differ from observed endpoint, NAT is detected
  - Learned public endpoints are stored and preferred for routing

- **Rendezvous Server**: Start with `--noforward` flag
  - Forwards route rumors to enable NAT traversal
  - Does NOT forward chat messages (only route rumors)
  - Useful for coordinating NATed nodes behind different NATs

### Message Flow Examples

- **Direct Routing**: Client1 sends private message to Client3 via DSDV routing table
  - If route exists: message forwarded through intermediate nodes using routing table
  - If no route: message broadcast to discover route, then routed once route is learned

- **Broadcast**: Messages with `Destination = "-1"` are delivered to all peers
  - Broadcast messages propagate through the network via anti-entropy

- **NAT Traversal**: NodeN1 (behind NAT1) and NodeN2 (behind NAT2) discover each other
  - Both connect to rendezvous server (NodeS)
  - Route rumors propagate with public endpoints
  - Nodes learn each other's public endpoints
  - Direct communication works despite NAT

## Testing the Implementation

### Automated Test Suite
The project includes basic test cases to verify functionality:

#### Running All Tests
```bash
# Run the complete test suite
./tests/run_tests.sh
```

#### Individual Test Files
```bash
# Test basic functionality (client init, UDP socket, discovery)
./tests/basic_functionality_tests.sh

# Test integration (multi-instance, broadcast, anti-entropy stability)
./tests/integration_tests.sh
```

### Basic Test Cases Included

#### 1. Basic Functionality Tests
- Client initialization with different configurations
- Command line options validation (help/version/client/port/peer)
- UDP socket binding on chosen port
- Local discovery across ports 9000-9009
- Manual peer addition via `--peer`
- Message format validation
- Rendezvous mode (`--noforward`) startup validation

#### 2. Integration Tests
- Multi-instance P2P network (4 clients)
- Direct messaging and broadcast delivery
- DSDV routing table establishment
- Anti-entropy synchronization (vector clock, sync_message)
- Retransmission and acks behavior
- GUI presence and stability under brief load

### NAT Traversal Testing (Linux Only)

The project includes a Linux network namespace setup script to simulate NAT environments:

```bash
sudo ./nat_test.sh
```

**Requirements**: Linux with `iproute2` and `iptables` installed. This script will exit with an error on macOS/Windows.

**What it does**:
1. Creates network namespaces (`nat1`, `nat2`, `bridge`)
2. Sets up virtual network interfaces and NAT rules
3. Launches rendezvous server (NodeS) with `--noforward` on host
4. Launches NodeN1 in NAT1 namespace with `--connect 45678`
5. Launches NodeN2 in NAT2 namespace with `--connect 45678`

**Expected behavior**:
- NodeN1 and NodeN2 connect to rendezvous server
- Route rumors propagate with public endpoints
- Nodes discover each other's public endpoints
- Direct communication works despite NAT
- Rendezvous server forwards route rumors but not chat messages

**Note**: If you're on macOS, run this test on a Linux host or VM.

### Network Namespace Setup Guide

For advanced testing, you can manually set up network namespaces:

```bash
# Create namespaces
sudo ip netns add nat1
sudo ip netns add nat2

# Create virtual ethernet pairs
sudo ip link add veth0 type veth peer name veth1
sudo ip link add veth2 type veth peer name veth3

# Move interfaces to namespaces
sudo ip link set veth1 netns nat1
sudo ip link set veth3 netns nat2

# Configure NAT1
sudo ip addr add 10.1.1.254/24 dev veth0
sudo ip link set veth0 up
sudo ip netns exec nat1 ip addr add 10.1.1.1/24 dev veth1
sudo ip netns exec nat1 ip link set veth1 up
sudo ip netns exec nat1 ip route add default via 10.1.1.254

# Configure NAT2
sudo ip addr add 10.2.2.254/24 dev veth2
sudo ip link set veth2 up
sudo ip netns exec nat2 ip addr add 10.2.2.1/24 dev veth3
sudo ip netns exec nat2 ip link set veth3 up
sudo ip netns exec nat2 ip route add default via 10.2.2.254

# Enable IP forwarding
echo 1 | sudo tee /proc/sys/net/ipv4/ip_forward

# Setup NAT rules
sudo iptables -t nat -A POSTROUTING -s 10.1.1.0/24 -j MASQUERADE
sudo iptables -t nat -A POSTROUTING -s 10.2.2.0/24 -j MASQUERADE

# Run SimpleChat in namespaces
sudo ip netns exec nat1 ./build/bin/SimpleChat --client NodeN1 --port 11111 --connect 45678
sudo ip netns exec nat2 ./build/bin/SimpleChat --client NodeN2 --port 22222 --connect 45678
```

### Manual Testing

#### Basic Functionality Test
1. Launch 4 instances using the launch script
2. Wait for discovery; the node list should populate with DSDV routes
3. Send a private message from Client1 to Client3 (double-click node or use dropdown)
4. Verify Client3 shows the message; check routing logs in intermediate nodes
5. Try Broadcast; all peers should display the message

#### DSDV Routing Test
1. Launch 4 clients in a line topology (Client1 → Client2 → Client3 → Client4)
2. Wait for route rumors to propagate (up to 60s)
3. Send private message from Client1 to Client4
4. Verify message is routed through intermediate nodes (check logs)
5. Verify routing table shows correct next hops and hop counts

#### NAT Traversal Test
1. Start rendezvous server: `./build/bin/SimpleChat --client NodeS --port 45678 --noforward`
2. Start NodeN1 in NAT1: `sudo ip netns exec nat1 ./build/bin/SimpleChat --client NodeN1 --port 11111 --connect 45678`
3. Start NodeN2 in NAT2: `sudo ip netns exec nat2 ./build/bin/SimpleChat --client NodeN2 --port 22222 --connect 45678`
4. Wait for route rumors to propagate
5. Verify nodes discover each other's public endpoints
6. Send private message from NodeN1 to NodeN2
7. Verify direct communication works despite NAT

#### Message Ordering and Anti-Entropy Test
1. Send multiple messages quickly from the same client
2. Temporarily start a client late or disconnect/reconnect
3. Verify late client catches up via anti-entropy (synced entries appear)

#### Network Resilience Test
1. Close one client instance and restart it
2. Observe discovery re-adds the peer automatically
3. Verify routing table updates with new routes
4. Verify missing messages sync via anti-entropy

## Technical Implementation Details

### Network Communication
- **UDP Socket**: Each client runs a QUdpSocket bound to its port
- **Discovery**: Periodic discovery messages to local ports 9000-9009 and manual `--peer`
- **Serialization**: QDataStream + QVariantMap with magic header (0xCAFEBABE) and size prefix
- **Protocol**: Message types include `message`, `private`, `route_rumor`, `ack`, `discovery`, `discovery_response`, `vector_clock`, `sync_message`

### DSDV Routing Implementation
- **Routing Table**: `QMap<QString, RouteEntry>` mapping destination → route information
  - RouteEntry contains: nextHop (IP), nextPort, sequenceNumber, hopCount, lastUpdate, isDirect, publicIP, publicPort
- **Route Updates**: When receiving route rumor or message:
  - If `SeqNo > currentSeqNo[Origin]`: update route to sender
  - Route preference: higher sequence > direct routes > fewer hops
- **Route Rumors**: Sent every 60 seconds (ROUTE_RUMOR_INTERVAL) and at startup
  - Forwarded to random neighbor (not back to sender)
  - Propagates through network to build routing topology

### Private Messaging with DSDV
- **Routing**: Private messages use `Dest` field to lookup routing table
- **Forwarding**: If route exists, forward to `route.nextHop:route.nextPort`
- **Hop Limit**: Decremented on each forward; message dropped if limit reaches 0
- **Fallback**: If no route found, message broadcast to discover route

### NAT Traversal
- **Public Endpoint Discovery**: All messages include `LastIP` and `LastPort` fields
- **NAT Detection**: If observed sender address differs from `LastIP`/`LastPort`, NAT is detected
- **Public Endpoint Storage**: `QMap<QString, QPair<QHostAddress, quint16>>` stores learned public endpoints
- **Route Preference**: Direct routes preferred when sequence numbers are equal

### Message Processing
- **Direct Routing**: Private messages routed via DSDV table if route exists
- **Broadcast**: Messages with `Destination = "-1"` delivered to all peers
- **Acks & Retransmission**: Pending acks tracked; messages resent after 2s if needed
- **Vector Clock**: Peers summarize max sequence per origin; missing messages are synced
- **Sequence Tracking**: Each origin maintains its own sequence numbers (chat messages) and DSDV sequence numbers (route rumors)

### Rendezvous Server Mode
- **No-Forward Mode**: `--noforward` flag sets `m_noForwardMode = true`
- **Behavior**: 
  - Chat messages (`Type == "message"`) are NOT forwarded
  - Route rumors (`Type == "route_rumor"`) ARE forwarded
  - Private messages are NOT forwarded (only route rumors propagate)

### Error Handling
- **Data Validation**: Magic header and size-checked QDataStream framing
- **Timeouts**: Periodic timers for discovery (5s), anti-entropy (3s), retransmission (2s), route rumors (60s)
- **Graceful Operation**: Missing peers time out (30s) and are removed from UI and routing table

## Troubleshooting

### Common Issues

#### "Failed to start server" Error
- **Cause**: Port already in use
- **Solution**: Kill existing processes: `pkill -f SimpleChat`
- **Prevention**: Always use the launch script which cleans up first

#### "Failed to connect to next peer" Error
- **Cause**: Target peer not started yet
- **Solution**: Wait a few seconds, the client will automatically retry
- **Check**: Ensure all clients are launched in the correct order

#### Messages Not Appearing
- **Check**: Ring connectivity - all clients should show "Connected" status
- **Verify**: Correct destination selection in dropdown
- **Debug**: Check message forwarding logs in intermediate clients

#### Build Failures
- **Qt6 Not Found**: Ensure Qt6 is installed and in PATH
- **CMake Issues**: Update CMake to version 3.16 or higher
- **Compiler Errors**: Ensure C++17 compatible compiler

### Debug Mode
To enable verbose logging, modify the application to include debug output:
```cpp
// Add to main.cpp
QLoggingCategory::setFilterRules("*.debug=true");
```

### Network Diagnostics
Check if ports are available:
```bash
# Linux/macOS
netstat -ln | grep 900[1-4]

# Windows  
netstat -an | findstr 900
```

## Advanced Features

### Adding More Clients
Simply start additional instances on unused local ports (e.g., 9005-9009). Pass `--peer` pointing to any known running peer to accelerate discovery.

### CLI Options
- `--client/-c <id>`: Unique identifier for the instance
- `--port/-p <port>`: UDP port to bind
- `--peer/-P <ip:port>`: Optional; can be repeated to send initial discovery to known peers
- `--noforward/-n`: Enable rendezvous server mode (forwards route rumors but not chat messages)
- `--connect/-C <port>`: Connect to rendezvous server at this port on localhost (for NAT testing)

### Message Encryption (Optional)
To add encryption, modify the serialization functions:
```cpp
// In serializeMessage()
QByteArray encryptedData = encrypt(data);
return encryptedData;

// In deserializeMessage()  
QByteArray decryptedData = decrypt(data);
// ... continue with normal deserialization
```

## Performance Considerations

### Network Latency
- **Peer Count**: More peers can increase propagation time across hops
- **Message Frequency**: High-frequency messaging may cause congestion
- **Buffer Management**: Qt event loop handles bursts; retransmission tuned at 2s

### Memory Usage
- **Message Queue**: Bounded to prevent memory leaks
- **Connection Pooling**: Reuses connections efficiently
- **GUI Updates**: Optimized text rendering for large chat logs

### Scalability Notes
- **Practical Local Ports**: Defaults to scanning 9000-9009; extend if needed
- **Vector Clock Growth**: Scales with number of origins
- **Routing Table Size**: Grows with number of nodes; each node stores routes to all known destinations
- **Route Rumor Frequency**: 60s interval balances convergence speed vs. network overhead


## Code Architecture

### Class Hierarchy
```
QMainWindow
└── SimpleChatP2P
    ├── UI Components (QTextEdit, QLineEdit, etc.)
    ├── Network Components (QUdpSocket)
    └── Message Management (Vector clock, timers, acks)
```

### Key Methods
- `setupUI()`: Initialize graphical interface with node list, private message button
- `setupNetwork()`: Configure UDP socket, timers (discovery, anti-entropy, retransmission, route rumors), and initial discovery
- `sendMessageToPeer()`: Serialize and send messages to a specific peer
- `sendPrivateMessage()`: Create and route private messages via DSDV table
- `sendRouteRumor()`: Generate and send route rumor to random neighbor
- `processRouteRumor()`: Process incoming route rumors and update routing table
- `updateRoutingTable()`: Update DSDV routing table with new route information
- `forwardPrivateMessage()`: Forward private messages with hop limit decrement
- `processNATInfo()`: Extract and store public endpoints from incoming messages
- `broadcastMessage()`: Send to all known peers
- `processReceivedMessage()`: Handle incoming messages (message, private, route_rumor, ack, discovery, vector_clock, sync)
- `performAntiEntropy()`: Exchange vector clocks and sync missing messages

### Thread Safety
- **Main Thread**: All GUI and network operations
- **Qt Event Loop**: Handles asynchronous network events
- **Signal/Slot Mechanism**: Thread-safe communication between components

## File Structure

```
simplechat/
├── main.cpp                    # Application entry point
├── simplechatp2p.h             # Main class header (DSDV routing, NAT traversal)
├── simplechatp2p.cpp           # Main class implementation  
├── CMakeLists.txt              # CMake build configuration
├── build.sh                    # Automated build script
├── launch_ring.sh              # P2P network launch script
├── nat_test.sh                 # NAT traversal testing script (Linux only)
├── README.md                   # This documentation
├── build_Instructions.md       # Build instructions and usage guide
├── dsdv_nat_documentation.md   # DSDV and NAT implementation details
└── tests/                      # Test suite directory
    ├── run_tests.sh                    # Test runner
    ├── basic_functionality_tests.sh    # Basic functionality tests (DSDV/NAT)
    └── integration_tests.sh            # Integration tests (DSDV/NAT)

```

## Submission Files

The complete submission includes:

1. **Source Code**
   - `main.cpp` - Application entry point
   - `simplechatp2p.h` - P2P class header
   - `simplechatp2p.cpp` - P2P implementation  

2. **Build Configuration**
   - `CMakeLists.txt` - CMake configuration
   - `build.sh` - Automated build script

3. **Deployment & Testing**
   - `launch_ring.sh` - Multi-instance launcher
   - `tests/run_tests.sh` - Test suite runner
   - `tests/basic_functionality_tests.sh` - Basic functionality test cases
   - `tests/integration_tests.sh` - Integration test cases

4. **Documentation**  
   - `README.md` - Comprehensive documentation, architecture, and troubleshooting information
   - `build_Instructions.md` - Build instructions, usage guide, network namespace setup, and known limitations
   - `dsdv_nat_documentation.md` - Detailed DSDV routing and NAT traversal implementation documentation

5. **Testing Scripts**
   - `nat_test.sh` - Linux network namespace setup for NAT traversal testing

## Known Limitations

1. **NAT Test Script Platform Dependency**
   - The `nat_test.sh` script requires Linux with `iproute2` and `iptables`
   - Not supported on macOS/Windows without a VM or container
   - The script will exit with an error message on unsupported platforms

2. **GUI Testing Limitations**
   - Automated tests are best-effort; headless CI cannot fully validate interactive message delivery
   - GUI window detection is platform-specific (macOS vs. Linux)

3. **Anti-Entropy Convergence**
   - Anti-entropy syncing is periodic (3s interval); very short-lived nodes may not fully converge
   - Messages may be missed if a node joins and leaves within the anti-entropy interval

4. **DSDV Route Convergence**
   - Route rumors propagate every 60s; initial routing table convergence may take up to 60s
   - In highly dynamic networks with frequent topology changes, route churn can occur
   - DSDV prefers direct routes at equal sequence numbers; indirect routes may not be optimal

5. **NAT Traversal Limitations**
   - NAT traversal relies on route rumor propagation; if rumors are blocked, traversal fails
   - Symmetric NAT may require additional coordination (not fully tested)
   - Public endpoint discovery depends on message exchange; isolated nodes won't discover each other

6. **Hop Limit**
   - Private messages have a default hop limit of 10; messages may be dropped in large networks
   - No automatic retry if message is dropped due to hop limit

7. **Message Ordering**
   - Messages are ordered per origin by sequence number; cross-origin ordering is not guaranteed
   - Vector clocks provide causal ordering within limitations of the protocol

## Testing Procedure Explanation

### Automated Test Suite
The test suite (`./tests/run_tests.sh`) validates:
1. **Basic Functionality**: Client initialization, CLI options, UDP binding, discovery, rendezvous mode
2. **Integration**: Multi-instance P2P network, DSDV routing table establishment, anti-entropy, GUI presence

### Manual Testing Workflow
1. **Basic P2P Test**: Launch 4 clients, verify discovery, send messages
2. **DSDV Routing Test**: Verify routing table updates, route rumors propagate, private messages routed correctly
3. **NAT Traversal Test** (Linux only): Use `nat_test.sh` to set up NAT environments, verify public endpoint discovery
4. **Rendezvous Server Test**: Launch rendezvous server, verify it forwards route rumors but not chat messages
5. **Anti-Entropy Test**: Start client late, verify it catches up via anti-entropy

### Validation Checklist
- [ ] Route rumors propagate within 60s
- [ ] Routing table shows correct next hops and hop counts
- [ ] Private messages routed through intermediate nodes
- [ ] NAT traversal works (public endpoints discovered)
- [ ] Rendezvous server forwards route rumors but not chat
- [ ] Anti-entropy syncs missing messages
- [ ] GUI shows node list with routing information
- [ ] Broadcast messages delivered to all peers
