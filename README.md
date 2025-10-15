# SimpleChat P2P - UDP-based Messaging Application

A Qt6-based distributed chat application that implements a peer-to-peer (P2P) and broadcast messaging system over UDP with anti-entropy synchronization and retransmission timers. This supersedes the earlier ring/TCP design.

## Architecture Overview

### P2P Network Model
- Each client listens on a UDP port and discovers peers via periodic local port discovery and optional manual peer addition.
- Messages are direct peer-to-peer when the destination is known; otherwise, they can be broadcast and/or propagated via anti-entropy.
- Anti-entropy uses a vector-clock-like summary so peers exchange missing messages across multiple hops.

### Key Concepts Implemented

1. **UDP Messaging**: Uses Qt's QUdpSocket for peer-to-peer and broadcast communication
2. **Message Serialization**: Messages are serialized using Qt's QDataStream and QVariantMap
3. **Peer Discovery**: Periodic local port discovery and manual IP:Port addition
4. **Sequence Numbering**: Per-origin sequence numbers for ordering and vector clock summarization
5. **Anti-Entropy Sync**: Periodic vector clock exchange to identify and send missing messages
6. **Retransmission Timers**: Unacknowledged messages are resent after short intervals

## Features

- **GUI Interface**: Qt6 UI with chat log, destination selector, and broadcast
- **UDP P2P & Broadcast**: Direct peer messaging; broadcast with Destination = "-1"
- **Discovery**: Automatic local port scan (9000-9009) and manual Add Peer (IP:Port)
- **Anti-Entropy**: Vector clock exchange and on-demand sync of missing messages
- **Reliability on UDP**: Acks and retransmissions (2s) for improved delivery
- **Message Ordering**: Per-origin sequence numbers maintained and summarized

## Message Format

All messages are QVariantMap-serialized via QDataStream with a magic header.

- Chat messages:
```cpp
{
    "Type": "message",
    "ChatText": "The actual message content",
    "Origin": "Unique sender ID",
    "Destination": "Peer ID or \"-1\" for broadcast",
    "Sequence": <int>,
    "Timestamp": <ms since epoch>
}
```

- Ack messages:
```cpp
{
    "Type": "ack",
    "Origin": "Ack sender ID",
    "AckOrigin": "Original message origin",
    "AckSequence": <int>
}
```

- Discovery / response:
```cpp
{ "Type": "discovery", "Origin": "<id>", "Port": <int> }
{ "Type": "discovery_response", "Origin": "<id>", "Port": <int> }
```

- Vector clock and sync:
```cpp
{ "Type": "vector_clock", "Origin": "<id>", "VectorClock": { "<origin>": <maxSeq>, ... } }
{ "Type": "sync_message", "Origin": "<id>", "SyncOrigin": "<origin>", "SyncSequence": <int>, "SyncDestination": "<id|-1>", "SyncText": "..." }
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

### Windows Installation
- Install Qt6 from [official website](https://www.qt.io/download)
- Install CMake from [cmake.org](https://cmake.org/download/)
- Use Visual Studio or MinGW for compilation

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

## Usage Instructions

1. Start the P2P network using `./launch_ring.sh` or manual launch as above
2. Wait for discovery: the status shows connected peers and the dropdown populates
3. Select a destination peer or use Broadcast
4. Type a message and click Send (or press Enter)
5. Peers exchange vector clocks periodically; missing messages are synced automatically

### Message Flow Examples
- Client1 sends a direct message to Client3 (if known): sent directly.
- If destination is unknown, message may propagate via broadcast and anti-entropy.
- Broadcast messages use Destination = -1 and are delivered to all peers.

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

#### 2. Integration Tests
- Multi-instance P2P network (4 clients)
- Direct messaging and broadcast delivery
- Anti-entropy synchronization (vector clock, sync_message)
- Retransmission and acks behavior
- GUI presence and stability under brief load

### Manual Testing

#### Basic Functionality Test
1. Launch 4 instances using the launch script
2. Wait for discovery; the peers list should populate in each window
3. Send a message from Client1 to Client3
4. Verify Client3 shows the message; check acks in sender log
5. Try Broadcast; all peers should display the message

#### Message Ordering and Anti-Entropy Test
1. Send multiple messages quickly from the same client
2. Temporarily start a client late or disconnect/reconnect
3. Verify late client catches up via anti-entropy (synced entries appear)

#### Network Resilience Test
1. Close one client instance and restart it
2. Observe discovery re-adds the peer automatically
3. Verify missing messages sync via anti-entropy

## Technical Implementation Details

### Network Communication
- **UDP Socket**: Each client runs a QUdpSocket bound to its port
- **Discovery**: Periodic discovery messages to local ports 9000-9009 and manual `--peer`
- **Serialization**: QDataStream + QVariantMap with magic header and size prefix
- **Protocol**: Message types include message, ack, discovery, discovery_response, vector_clock, sync_message

### Message Processing
- **Direct and Broadcast**: Messages sent directly if destination peer known, else broadcast
- **Acks & Retransmission**: Pending acks tracked; messages resent after 2s if needed
- **Vector Clock**: Peers summarize max sequence per origin; missing messages are synced
- **Sequence Tracking**: Each origin maintains its own sequence numbers

### Error Handling
- **Data Validation**: Magic header and size-checked QDataStream framing
- **Timeouts**: Periodic timers for discovery, anti-entropy, and retransmission
- **Graceful Operation**: Missing peers time out and are removed from the UI

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
- `setupUI()`: Initialize graphical interface
- `setupNetwork()`: Configure UDP socket, timers, and initial discovery
- `sendMessageToPeer()`: Serialize and send messages to a specific peer
- `broadcastMessage()`: Send to all known peers
- `processReceivedMessage()`: Handle incoming messages, acks, vector clocks, sync
- `performAntiEntropy()`: Exchange vector clocks and sync missing messages

### Thread Safety
- **Main Thread**: All GUI and network operations
- **Qt Event Loop**: Handles asynchronous network events
- **Signal/Slot Mechanism**: Thread-safe communication between components

## File Structure

```
simplechat/
├── main.cpp              # Application entry point
├── simplechatp2p.h          # Main class header
├── simplechatp2p.cpp        # Main class implementation  
├── CMakeLists.txt        # CMake build configuration
├── build.sh              # Automated build script
├── launch_ring.sh        # Ring network launch script
├── README.md             # This documentation
├── build_instructions.md # Build instructions and usage guide
└── tests/                # Test suite directory
    ├── run_tests.sh                    # Test runner
    ├── basic_functionality_tests.sh    # Basic functionality tests
    └── integration_tests.sh            # Integration tests

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
   - `README.md` - Comprehensive documentation, Architecture and troubleshooting information
   - `build_instructions.md` - Build instructions and usage guide, 
