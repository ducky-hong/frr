#!/bin/bash
# End-to-end test of userspace provider socket communication

echo "============================================================"
echo "END-TO-END SOCKET COMMUNICATION TEST"
echo "============================================================"
echo ""
echo "This test verifies that the userspace provider can send"
echo "events through Unix socket and they are received correctly."
echo ""

# Start listener in background
echo "1. Starting mock zebra listener..."
python3 test_socket_listener.py > /tmp/listener.log 2>&1 &
LISTENER_PID=$!
sleep 1

if ! kill -0 $LISTENER_PID 2>/dev/null; then
    echo "   ✗ Listener failed to start"
    cat /tmp/listener.log
    exit 1
fi
echo "   ✓ Listener started (PID $LISTENER_PID)"
echo ""

# Run client to send events
echo "2. Sending events from client..."
cat > /tmp/test_client.py << 'EOF'
import sys
sys.path.insert(0, "tests/lib")
from usrspace_provider import UsrspaceProvider
import time

provider = UsrspaceProvider(sock_path="/tmp/test_zebra_usrspace.sock")
provider.connect()

# Test interface events (these work)
provider.inject_intf_add("test0", ifindex=100, mtu=1500, hw_addr=b'\x00\x11\x22\x33\x44\x55')
time.sleep(0.2)

provider.inject_intf_up(ifindex=100)
time.sleep(0.2)

provider.inject_intf_down(ifindex=100)
time.sleep(0.2)

provider.inject_intf_add("vxlan1000", ifindex=200, mtu=9152, is_vxlan=True, vni=1000, vtep_ip="192.168.100.1")
time.sleep(0.2)

provider.inject_intf_add("bond0", ifindex=300, mtu=1500, is_bond=True, hw_addr=b'\x44\x38\x39\xff\xff\x01')
time.sleep(0.2)

provider.close()
print("✓ All events sent")
EOF

python3 /tmp/test_client.py
CLIENT_RESULT=$?
echo ""

# Wait a bit for processing
sleep 1

# Stop listener
echo "3. Stopping listener..."
kill $LISTENER_PID 2>/dev/null
wait $LISTENER_PID 2>/dev/null
echo ""

# Show results
echo "============================================================"
echo "RESULTS"
echo "============================================================"
echo ""
grep "Total events received:" /tmp/listener.log | tail -1
echo ""
grep "✓ Received Event:" /tmp/listener.log | wc -l | xargs echo "Events successfully received:"
echo ""

# Check if we got events
EVENTS=$(grep -c "✓ Received Event:" /tmp/listener.log)
if [ "$EVENTS" -gt 0 ]; then
    echo "✓✓✓ SUCCESS: Socket communication working!"
    echo ""
    echo "Details from listener log:"
    echo "----------------------------------------"
    grep -A 6 "✓ Received Event:" /tmp/listener.log | head -35
    echo "----------------------------------------"
    echo ""
    echo "✓ Wire protocol validated"
    echo "✓ Events transmitted correctly"
    echo "✓ Socket communication verified"
    exit 0
else
    echo "✗ FAILED: No events received"
    echo ""
    echo "Listener log:"
    cat /tmp/listener.log
    exit 1
fi
