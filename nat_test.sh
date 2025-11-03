#!/bin/bash

# NAT Traversal Testing Script for SimpleChat
# This script creates network namespaces to simulate NAT environments

echo "=== SimpleChat NAT Traversal Testing Setup ==="

# Platform check: this script requires Linux (ip, iptables, netns)
UNAME=$(uname -s 2>/dev/null)
if [ "$UNAME" != "Linux" ]; then
    echo "Error: This NAT test script requires Linux with iproute2 and iptables."
    echo "You're running on $UNAME. macOS does not provide 'ip' or Linux namespaces."
    echo "Options:"
    echo "  - Run on a Linux host or within a Linux VM (e.g., Ubuntu)"
    echo "  - Or use Docker to run a Linux container with --privileged and iproute2/iptables installed"
    exit 1
fi

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "Please run as root (sudo)"
    exit 1
fi

BUILD_DIR="./build/bin"

# Check if executable exists
if [ ! -f "$BUILD_DIR/SimpleChat" ]; then
    echo "Error: SimpleChat executable not found at $BUILD_DIR/SimpleChat"
    echo "Please build the project first using build.sh"
    exit 1
fi

# Function to cleanup
cleanup() {
    echo "Cleaning up..."
    
    # Kill any existing SimpleChat processes
    pkill -f "SimpleChat" 2>/dev/null
    
    # Delete network namespaces
    ip netns del nat1 2>/dev/null
    ip netns del nat2 2>/dev/null
    ip netns del bridge 2>/dev/null
    
    # Delete virtual ethernet pairs
    ip link del veth0 2>/dev/null
    ip link del veth2 2>/dev/null
    ip link del veth4 2>/dev/null
    
    # Delete bridge
    ip link del br0 2>/dev/null
    
    echo "Cleanup complete"
}

# Trap cleanup on exit
trap cleanup EXIT

## Tooling checks
for TOOL in ip iptables; do
    if ! command -v $TOOL >/dev/null 2>&1; then
        echo "Error: Required tool '$TOOL' not found. Install iproute2 and iptables."
        exit 1
    fi
done

# Initial cleanup (ignore errors)
cleanup || true

echo "Creating network namespaces..."

# Create namespaces
ip netns add nat1
ip netns add nat2
ip netns add bridge

echo "Creating virtual network interfaces..."

# Create veth pairs
ip link add veth0 type veth peer name veth1
ip link add veth2 type veth peer name veth3
ip link add veth4 type veth peer name veth5

# Create bridge in bridge namespace
ip link add br0 type bridge

# Move interfaces to namespaces
ip link set veth1 netns nat1
ip link set veth3 netns nat2
ip link set veth5 netns bridge
ip link set br0 netns bridge

# Configure bridge namespace
echo "Configuring bridge namespace..."
ip netns exec bridge ip link set br0 up
ip netns exec bridge ip addr add 192.168.100.1/24 dev br0
ip netns exec bridge ip link set veth5 up
ip netns exec bridge ip link set veth5 master br0

# Configure host side
ip addr add 192.168.100.254/24 dev veth4
ip link set veth4 up

# Configure NAT1
echo "Configuring NAT1..."
ip link set veth0 up
ip addr add 10.1.1.254/24 dev veth0
ip netns exec nat1 ip link set lo up
ip netns exec nat1 ip link set veth1 up
ip netns exec nat1 ip addr add 10.1.1.1/24 dev veth1
ip netns exec nat1 ip route add default via 10.1.1.254

# Configure NAT2
echo "Configuring NAT2..."
ip link set veth2 up
ip addr add 10.2.2.254/24 dev veth2
ip netns exec nat2 ip link set lo up
ip netns exec nat2 ip link set veth3 up
ip netns exec nat2 ip addr add 10.2.2.1/24 dev veth3
ip netns exec nat2 ip route add default via 10.2.2.254

# Enable IP forwarding
echo 1 > /proc/sys/net/ipv4/ip_forward

# Setup NAT rules
echo "Setting up NAT rules..."

# NAT for namespace nat1 (symmetric NAT)
iptables -t nat -A POSTROUTING -s 10.1.1.0/24 -o veth4 -j MASQUERADE
iptables -A FORWARD -i veth0 -o veth4 -j ACCEPT
iptables -A FORWARD -i veth4 -o veth0 -m state --state RELATED,ESTABLISHED -j ACCEPT

# NAT for namespace nat2 (port-restricted cone NAT)
iptables -t nat -A POSTROUTING -s 10.2.2.0/24 -o veth4 -j MASQUERADE --random
iptables -A FORWARD -i veth2 -o veth4 -j ACCEPT
iptables -A FORWARD -i veth4 -o veth2 -m state --state RELATED,ESTABLISHED -j ACCEPT

echo "Network setup complete!"
echo ""
echo "=== Starting SimpleChat Instances ==="
echo ""

# Start rendezvous server (no-forward mode)
echo "Starting rendezvous server (NodeS) on host..."
$BUILD_DIR/SimpleChat --client NodeS --port 45678 --noforward &
RENDEZVOUS_PID=$!
sleep 2

# Start NAT1 client
echo "Starting NodeN1 in NAT1 namespace..."
ip netns exec nat1 $BUILD_DIR/SimpleChat --client NodeN1 --port 11111 --connect 45678 &
NAT1_PID=$!
sleep 2

# Start NAT2 client
echo "Starting NodeN2 in NAT2 namespace..."
ip netns exec nat2 $BUILD_DIR/SimpleChat --client NodeN2 --port 22222 --connect 45678 &
NAT2_PID=$!
sleep 2

echo ""
echo "=== NAT Traversal Test Environment Ready ==="
echo ""
echo "Rendezvous Server (NodeS): Running on host at port 45678 [NO-FORWARD mode]"
echo "NodeN1: Running in NAT1 namespace (10.1.1.1) on port 11111"
echo "NodeN2: Running in NAT2 namespace (10.2.2.1) on port 22222"
echo ""
echo "Expected behavior:"
echo "1. NodeN1 and NodeN2 connect to rendezvous server"
echo "2. They discover each other's public endpoints via route rumors"
echo "3. Direct communication should work despite NAT"
echo "4. Rendezvous server won't forward chat messages (only route rumors)"
echo ""
echo "To view network namespace details:"
echo "  ip netns exec nat1 ip addr"
echo "  ip netns exec nat2 ip addr"
echo "  iptables -t nat -L -n -v"
echo ""
echo "Press Ctrl+C to stop all instances and cleanup"
echo ""

# Wait for processes
wait $RENDEZVOUS_PID $NAT1_PID $NAT2_PID