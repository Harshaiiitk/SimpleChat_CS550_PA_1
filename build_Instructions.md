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

2. **Make build script executable**:
   ```bash
   chmod +x build.sh
   chmod +x launch_ring.sh
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
This launches all 4 clients in the correct ring topology.

### Manual Launch
Launch each client in separate terminals:

```bash
# Terminal 1
./build/bin/SimpleChat --client Client1 --listen 9001 --target 9002

# Terminal 2  
./build/bin/SimpleChat --client Client2 --listen 9002 --target 9003

# Terminal 3
./build/bin/SimpleChat --client Client3 --listen 9003 --target 9004

# Terminal 4
./build/bin/SimpleChat --client Client4 --listen 9004 --target 9001
```

## Usage Instructions

1. **Start the ring network** using `./launch_ring.sh`
2. **Wait for connections**: All clients should show "Connected" status
3. **Select destination**: Use the dropdown to choose message recipient
4. **Type message**: Enter your message in the input field
5. **Send**: Click "Send" or press Enter

### Message Flow Example
- Client1 sends message to Client3
- Message travels: Client1 → Client2 → Client3
- Client2 forwards the message automatically
- Client3 receives and displays the message

## Testing the Implementation

### Basic Functionality Test
1. Launch 4 instances using the launch/test script
2. Wait for all connections to establish (status shows "Connected")
3. Send a message from Client1 to Client3
4. Verify message appears in Client3's chat log
5. Verify Client2 shows forwarding information

### Message Ordering Test
1. Send multiple messages quickly from the same client
2. Verify messages arrive in the correct sequence order
3. Check sequence numbers in the forwarding logs

### Network Resilience Test
1. Close one client instance
2. Verify other clients attempt reconnection
3. Restart the closed client
4. Verify ring network re-establishes automatically

## File Structure

```
simplechat/
├── main.cpp               # Application entry point
├── simplechat.h           # Main class header
├── simplechat.cpp         # Main class implementation  
├── CMakeLists.txt         # CMake build configuration
├── build.sh               # Automated build script
├── launch_ring.sh         # Ring network launch script
├── test_ring.sh           # Ring network test script
└── README.md              # Comprehensive documentation
└── build_instructions.md  # Build instructions and usage guide
```