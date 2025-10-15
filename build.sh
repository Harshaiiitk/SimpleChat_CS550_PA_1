#!/bin/bash

# SimpleChat Build Script

echo "Building SimpleChat..."

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

if [ $? -ne 0 ]; then
    echo "CMake configuration failed!"
    exit 1
fi

# Build
echo "Compiling..."
# Use sysctl on macOS to get CPU count, fallback to 4 if not available
if command -v nproc &> /dev/null; then
    CORES=$(nproc)
elif command -v sysctl &> /dev/null; then
    CORES=$(sysctl -n hw.ncpu)
else
    CORES=4
fi
make -j$CORES

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo "Build successful!"
echo "Executable created at: ./build/bin/SimpleChat"
echo ""
echo "To launch multiple P2P instances, run: ./launch_ring.sh"
echo "Or launch manually (UDP discovery + optional peers):"
echo "  ./build/bin/SimpleChat --client Client1 --port 9001"
echo "  ./build/bin/SimpleChat --client Client2 --port 9002 --peer 127.0.0.1:9001"