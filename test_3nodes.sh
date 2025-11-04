#!/bin/bash

# Core Functionality Test - 3 Nodes as per Requirements
# Run 3 nodes locally as specified in requirements:
# ./simplechat -port 12345 & # NodeA
# ./simplechat -port 23456 & # NodeB
# ./simplechat -port 34567 & # NodeC

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"

# Change to project root
cd "$PROJECT_ROOT"

echo "=========================================="
echo "Core Functionality Test - 3 Nodes"
echo "=========================================="

BUILD_DIR="./build/bin"

# Check if executable exists
if [ ! -f "$BUILD_DIR/SimpleChat" ]; then
    echo "Error: SimpleChat executable not found at $BUILD_DIR/SimpleChat"
    echo "Please build the project first using build.sh"
    exit 1
fi

# Function to cleanup
cleanup() {
    echo ""
    echo "Cleaning up test processes..."
    pkill -f "SimpleChat" 2>/dev/null
    sleep 2
}

# Set up cleanup trap
trap cleanup EXIT

# Clean up any existing instances
cleanup

echo ""
echo "Launching 3 nodes as per requirements:"
echo "  NodeA on port 12345"
echo "  NodeB on port 23456"
echo "  NodeC on port 34567"
echo ""

# Launch NodeA
echo "Starting NodeA (port 12345)..."
$BUILD_DIR/SimpleChat --client NodeA --port 12345 &
NODEA_PID=$!
sleep 1

# Launch NodeB
echo "Starting NodeB (port 23456)..."
$BUILD_DIR/SimpleChat --client NodeB --port 23456 --peer 127.0.0.1:12345 &
NODEB_PID=$!
sleep 1

# Launch NodeC
echo "Starting NodeC (port 34567)..."
$BUILD_DIR/SimpleChat --client NodeC --port 34567 --peer 127.0.0.1:12345 &
NODEC_PID=$!
sleep 2

# Verify all nodes are running
echo ""
echo "Verifying all nodes are running..."
ALL_RUNNING=true
for PID in $NODEA_PID $NODEB_PID $NODEC_PID; do
    if ! kill -0 $PID 2>/dev/null; then
        echo "✗ Node with PID $PID is not running"
        ALL_RUNNING=false
    fi
done

if [ "$ALL_RUNNING" = false ]; then
    echo "✗ FAIL: Some nodes failed to start"
    exit 1
fi

echo "✓ All 3 nodes are running successfully"
echo ""
echo "=========================================="
echo "Verification Steps"
echo "=========================================="
echo ""
echo "Verify the following (in GUI windows or logs):"
echo ""
echo "1. Route rumors propagate within 60s:"
echo "   - Wait up to 60 seconds"
echo "   - Check node list in each GUI window"
echo "   - Verify nodes appear in each other's node list"
echo "   - Route rumors are sent every 60s"
echo ""
echo "2. Private messages route through intermediates:"
echo "   - In NodeA: Send private message to NodeC"
echo "   - Verify message is routed through NodeB (if NodeB is intermediate)"
echo "   - Check NodeB logs/GUI for forwarding activity"
echo "   - Verify NodeC receives the message"
echo ""
echo "3. Routing table:"
echo "   - Check node list shows discovered nodes with sequence numbers"
echo "   - Verify routing table is populated"
echo ""
echo "Nodes will continue running. Press Ctrl+C to stop and cleanup."
echo ""

# Wait for user interrupt
wait

