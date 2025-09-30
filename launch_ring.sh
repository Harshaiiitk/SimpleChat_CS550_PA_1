#!/bin/bash

# SimpleChat Ring Network Launch Script
# This script launches 4 instances of SimpleChat in a ring configuration

echo "Starting SimpleChat Ring Network..."

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
echo "Launching Client1 (9001 -> 9002)..."
$BUILD_DIR/SimpleChat --client Client1 --listen 9001 --target 9002 &
sleep 0.5

echo "Launching Client2 (9002 -> 9003)..."
$BUILD_DIR/SimpleChat --client Client2 --listen 9002 --target 9003 &
sleep 0.5

echo "Launching Client3 (9003 -> 9004)..."
$BUILD_DIR/SimpleChat --client Client3 --listen 9003 --target 9004 &
sleep 0.5

echo "Launching Client4 (9004 -> 9001)..."
$BUILD_DIR/SimpleChat --client Client4 --listen 9004 --target 9001 &
sleep 0.5

echo "All instances launched!"
echo "Ring topology: Client1:9001 -> Client2:9002 -> Client3:9003 -> Client4:9004 -> Client1:9001"
echo ""
echo "To stop all instances, run: pkill -f SimpleChat"
echo "Press Ctrl+C to stop this script (instances will continue running)"

# Keep script running to show status
wait