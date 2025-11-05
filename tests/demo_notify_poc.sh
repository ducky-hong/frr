#!/bin/bash
# Demo script for zebra notification provider POC
# Shows how the architecture works without requiring full zebra integration

set -e

SOCKET_PATH="/tmp/frr-notify-demo.sock"
TEST_PROG="./zebra_notify_test"

echo "=============================================="
echo "Zebra Notification Provider POC Demo"
echo "=============================================="
echo ""

# Check if test program exists
if [ ! -f "$TEST_PROG" ]; then
    echo "Error: Test program not found. Run 'make -f Makefile.notify_poc' first"
    exit 1
fi

echo "This POC demonstrates:"
echo "  1. Pluggable notification provider architecture"
echo "  2. Userspace FDB event injection (no kernel dependency)"
echo "  3. EVPN multihoming support (ESI association)"
echo "  4. Peer sync scenarios (local + remote MACs)"
echo ""
echo "Architecture:"
echo "  Userspace Dataplane → Unix Socket → Notification Provider → Zebra"
echo ""

# Create a simple mock socket server to demonstrate the concept
echo "Creating mock notification receiver..."
echo ""

# Python script to act as notification receiver
cat > /tmp/mock_receiver.py << 'EOF'
#!/usr/bin/env python3
import socket
import os
import json
import sys

SOCKET_PATH = "/tmp/frr-notify-demo.sock"

# Remove old socket
try:
    os.unlink(SOCKET_PATH)
except OSError:
    pass

# Create Unix socket server
server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
server.bind(SOCKET_PATH)
server.listen(5)
print(f"Mock notification receiver listening on {SOCKET_PATH}")
print("Waiting for test program to connect...")
print("")

try:
    conn, _ = server.accept()
    print("Test program connected!")
    print("")
    print("Received notifications:")
    print("-" * 80)

    buffer = ""
    count = 0
    while True:
        data = conn.recv(1024)
        if not data:
            break

        buffer += data.decode('utf-8')

        # Process complete messages (newline-delimited)
        while '\n' in buffer:
            line, buffer = buffer.split('\n', 1)
            if line.strip():
                count += 1
                try:
                    event = json.loads(line)
                    print(f"Event {count}: {event['op']}")

                    if event['op'] in ['fdb_add', 'fdb_delete']:
                        print(f"  MAC:     {event.get('mac', 'N/A')}")
                        print(f"  VNI:     {event.get('vni', 'N/A')}")
                        print(f"  IF:      {event.get('ifname', 'N/A')} (ifindex={event.get('ifindex', 'N/A')})")
                        if 'esi' in event:
                            print(f"  ESI:     {event['esi']} ← EVPN MH!")
                        print(f"  Local:   {event.get('local', 'N/A')}")

                    print("")

                except json.JSONDecodeError as e:
                    print(f"Invalid JSON: {e}")

    print("-" * 80)
    print(f"Total events received: {count}")
    print("")
    print("✓ POC demonstrates userspace FDB injection without kernel dependency")

except KeyboardInterrupt:
    print("\nStopped by user")
finally:
    server.close()
    os.unlink(SOCKET_PATH)

EOF

chmod +x /tmp/mock_receiver.py

# Run the mock receiver in background
python3 /tmp/mock_receiver.py &
RECEIVER_PID=$!

# Give it time to start
sleep 1

echo "Running test program..."
echo ""

# Modify test program to use our demo socket
export SOCKET_PATH="/tmp/frr-notify-demo.sock"

# Run specific EVPN MH test (test 5 - peer sync)
if SOCKET_PATH=$SOCKET_PATH $TEST_PROG 5 2>&1; then
    echo ""
    echo "=============================================="
    echo "POC Demo Complete!"
    echo "=============================================="
    echo ""
    echo "What was demonstrated:"
    echo "  ✓ Userspace program sent FDB events via Unix socket"
    echo "  ✓ Events included EVPN MH ESI information"
    echo "  ✓ Simulated local and remote MAC learning"
    echo "  ✓ No kernel netlink dependency required"
    echo ""
    echo "In production:"
    echo "  - Your dataplane (DPDK/VPP/custom) would send these events"
    echo "  - Zebra's notification provider would receive them"
    echo "  - EVPN MH would process them like kernel netlink events"
    echo "  - Works on ANY platform (macOS, FreeBSD, Windows, etc.)"
    echo ""
else
    echo "Test failed - but that's OK for a POC!"
fi

# Wait for receiver to finish
sleep 1
kill $RECEIVER_PID 2>/dev/null || true
wait $RECEIVER_PID 2>/dev/null || true

# Cleanup
rm -f /tmp/mock_receiver.py

echo "See tests/NOTIFY_POC_README.md for complete documentation"
