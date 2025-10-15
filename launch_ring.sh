#!/bin/bash

# SimpleChat P2P Launch Script
# This script launches 4 instances of SimpleChat using UDP P2P + discovery

echo "Starting SimpleChat P2P Network..."

# Build directory path
BUILD_DIR="./build/bin"

# Check if executable exists
if [ ! -f "$BUILD_DIR/SimpleChat" ]; then
    echo "Error: SimpleChat executable not found at $BUILD_DIR/SimpleChat"
    echo "Please build the project first using build.sh"
    exit 1
fi

# Kill any existing instances
echo "Cleaning up any existing instances..."
pkill -f "SimpleChat" 2>/dev/null

# Wait a moment for cleanup
sleep 1

# Launch instances in background
echo "Launching Client1 (9001)..."
$BUILD_DIR/SimpleChat --client Client1 --port 9001 &
sleep 0.5

echo "Launching Client2 (9002, peer 9001)..."
$BUILD_DIR/SimpleChat --client Client2 --port 9002 --peer 127.0.0.1:9001 &
sleep 0.5

echo "Launching Client3 (9003, peer 9001)..."
$BUILD_DIR/SimpleChat --client Client3 --port 9003 --peer 127.0.0.1:9001 &
sleep 0.5

echo "Launching Client4 (9004, peer 9001)..."
$BUILD_DIR/SimpleChat --client Client4 --port 9004 --peer 127.0.0.1:9001 &
sleep 0.5

echo "All instances launched!"
echo "Each peer does UDP discovery on local ports 9000-9009."
echo ""
echo "To stop all instances, run: pkill -f SimpleChat"
echo "Press Ctrl+C to stop this script (instances will continue running)"

# Keep script running to show status
wait