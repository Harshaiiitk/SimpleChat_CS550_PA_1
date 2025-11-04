# Testing Guide

This document describes all test scripts available for SimpleChat DSDV/NAT implementation.

## Test Scripts Overview

### Core Functionality Tests

#### 1. `test_3nodes.sh` - Simple 3-Node Test (Requirements Format)
**Purpose**: Launch 3 nodes exactly as specified in requirements

**Usage**:
```bash
./test_3nodes.sh
```

**What it does**:
- Launches NodeA on port 12345
- Launches NodeB on port 23456 (connects to NodeA)
- Launches NodeC on port 34567 (connects to NodeA)

- Here connecting to other nodes should be done manually by adding the peer using "Add Peer" option because the the ports are not in the discovery range.

**Verification** (manual):
- Route rumors propagate within 60s
- Private messages route through intermediates

---


### NAT Traversal Tests

#### 2. `nat_test.sh` - NAT Environment Setup
**Purpose**: Sets up Linux network namespaces for NAT testing

**Usage** (Linux only):
```bash
sudo ./nat_test.sh
```

**What it does**:
- Creates network namespaces (nat1, nat2, bridge)
- Configures NAT rules inside namespaces (as per requirement 2.1)
- Launches rendezvous server (NodeS) with `--noforward` on port 45678
- Launches NodeN1 in NAT1 namespace on port 11111
- Launches NodeN2 in NAT2 namespace on port 22222

**Requirements**:
- Linux with `iproute2` and `iptables`
- Root privileges (sudo)

**Note**: This script sets up the environment. Verification is done separately.

---

## Known Limitations

- NAT test script requires Linux (iproute2 + iptables). Not supported on macOS without a VM/containers.
- GUI-based tests are best-effort; headless CI cannot validate interactive delivery.
- Anti-entropy syncing is periodic; very short-lived nodes may not fully converge.
- DSDV prefers direct routes at equal sequence numbers; in highly dynamic networks, route churn can occur.


## Test Workflow

### Core Functionality Testing

**Option 1: Quick Test (Requirements Format)**
```bash
./test_3nodes.sh
# Then manually verify in GUI windows
```

**Option 2: Full Test Suite**
```bash
./tests/run_tests.sh
# Runs all automated tests
```

---

### NAT Traversal Testing

**Step 1: Set up NAT environment**
```bash
sudo ./nat_test.sh
# This starts all nodes in NAT environment
```

**Or manually verify**:
1. Check GUI windows for NodeN1 and NodeN2
2. Verify node list shows discovered nodes
3. Look for [NAT] markers indicating public endpoints
4. Send private message from NodeN1 to NodeN2
5. Verify direct messaging works

---

## Requirements Mapping

### Requirement: Core Functionality Tests
```bash
# Run 3 nodes locally
./simplechat -port 12345 & # NodeA
./simplechat -port 23456 & # NodeB
./simplechat -port 34567 & # NodeC
```

**Implementation**: `test_3nodes.sh` or `tests/test_core_functionality.sh`

**Verify**:
- ✅ Route rumors propagate within 60s
- ✅ Private messages route through intermediates

---

### Requirement: NAT Traversal Tests
```bash
# Rendezvous server
./simplechat -port 45678 -noforward & # NodeS
# NATted nodes
ip netns exec nat1 ./simplechat -port 11111 -connect 45678 & # NodeN1
ip netns exec nat2 ./simplechat -port 22222 -connect 45678 & # NodeN2
```

**Implementation**: `nat_test.sh` (sets up environment)

**Verify**:
- ✅ NodeN1/N2 discover each other's public endpoints
- ✅ Direct messaging works despite NodeS's -noforward

**Verification Script**: `tests/test_nat_traversal.sh`

---

## Test Script Summary

| Script | Purpose | Platform | Requires Root | Notes |
|--------|---------|----------|----------------|-------|
| `test_3nodes.sh` | Launch 3 nodes (requirements format) | All | No | Simple launcher |
| `nat_test.sh` | NAT environment setup | Linux only | Yes | Sets up namespaces |
| `tests/run_tests.sh` | Run all automated tests | All | No | Test suite runner |

---

## Manual Verification Checklist

### NAT Traversal

- [ ] Rendezvous server (NodeS) starts with `--noforward`
- [ ] NodeN1 and NodeN2 connect to rendezvous server
- [ ] Node list shows discovered nodes with [NAT] markers
- [ ] Public endpoints are discovered (check logs for "NAT detected")
- [ ] Private message from NodeN1 to NodeN2 works
- [ ] Direct messaging works (not routed through NodeS)
- [ ] NodeS forwards route rumors but NOT chat messages

---

## Troubleshooting

### Nodes not discovering each other
- Wait up to 60 seconds for route rumors to propagate
- Check if ports are available (not already in use)
- Verify firewall is not blocking UDP traffic

### NAT test fails
- Ensure you're on Linux
- Check if `iproute2` and `iptables` are installed
- Verify you have root privileges
- Run `nat_test.sh` first before `test_nat_traversal.sh`

### GUI windows not showing
- Check if Qt6 is properly installed
- Verify DISPLAY is set (for Linux with X11)
- Check logs for error messages

---

## Quick Reference

```bash
# Core functionality - quick test
./test_3nodes.sh

# NAT traversal - setup (Linux only)
sudo ./nat_test.sh

# Run all automated tests
./tests/run_tests.sh
```

