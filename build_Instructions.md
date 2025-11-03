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

Note: Linux network namespaces (for NAT simulation) are not available on macOS. Run NAT tests on Linux or in a Linux VM.

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
This launches all 4 clients using UDP P2P with discovery.

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

1. **Start the P2P network** using `./launch_ring.sh`
2. **Wait for discovery**: All clients should show connected peers in status
3. **Select destination**: Use the dropdown to choose message recipient or select Broadcast
4. **Type message**: Enter your message in the input field
5. **Send**: Click "Send" or press Enter

### Message Flow Example
- DSDV routing auto-discovers next hops via periodic route rumors
- Private messages use `Dest`, `HopLimit`, and the DSDV table for forwarding
- If no route is known, messages may be broadcast while routes converge
- Broadcast messages (Destination = "-1") are delivered to all peers

### DSDV Routing & Private Messaging
- Route rumors: sent every 60s and at startup, with fields: `{"Type":"route_rumor","Origin":"<NodeID>","SeqNo":N}`
- Routing table update: prefer higher sequence numbers; otherwise prefer direct routes, then fewer hops
- Private messages: `{"Type":"private","Origin":"A","Dest":"B","ChatText":"...","HopLimit":10}`
- GUI: node list shows discovered destinations with sequence and hop metrics; double-click to PM

### NAT Traversal & Rendezvous Mode
- All messages include `LastIP` and `LastPort` for public endpoint learning
- Learned public endpoints are preferred for direct routing when available
- Rendezvous: start a node with `--noforward` to avoid forwarding chat (still forwards rumors)

## Testing the Implementation

### Automated Test Suite
The project includes tests aligned with DSDV, private messaging, and anti-entropy:

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
- 4-node P2P network spin-up
- Broadcast/direct delivery readiness checks
- Anti-entropy (vector clock) presence
- GUI process/window presence checks (best effort per OS)
- Stability while running briefly

### NAT Traversal Testing (Linux only)
Run the Linux network namespace setup to simulate NAT types:
```bash
sudo ./nat_test.sh
```
This creates namespaces, sets up NAT (masquerade), launches a rendezvous (`--noforward`) and two NATed clients (`--connect <rendezvousPort>`). Verify that public endpoints are discovered and direct messaging works via DSDV.

If you are on macOS/Windows, run the NAT test on a Linux host or VM.

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

## Known Limitations

- NAT test script requires Linux (iproute2 + iptables). Not supported on macOS without a VM/containers.
- GUI-based tests are best-effort; headless CI cannot validate interactive delivery.
- Anti-entropy syncing is periodic; very short-lived nodes may not fully converge.
- DSDV prefers direct routes at equal sequence numbers; in highly dynamic networks, route churn can occur.

## File Structure

```
simplechat/
├── main.cpp              # Application entry point
├── simplechatp2p.h       # Main class header
├── simplechatp2p.cpp     # Main class implementation  
├── CMakeLists.txt        # CMake build configuration
├── build.sh              # Automated build script
├── launch_ring.sh        # P2P network launch script
├── README.md             # This documentation
├── build_instructions.md # Build instructions and usage guide
└── tests/                # Test suite directory
    ├── run_tests.sh                    # Test runner
    ├── basic_functionality_tests.sh    # Basic functionality tests
    └── integration_tests.sh            # Integration tests
```