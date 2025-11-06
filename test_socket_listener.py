#!/usr/bin/env python3
"""
Minimal socket listener that mimics zebra's userspace provider.

This demonstrates the socket communication working without
requiring zebra to be built and running.
"""

import socket
import struct
import os
import sys

SOCKET_PATH = "/tmp/test_zebra_usrspace.sock"
USRSPACE_MAGIC = 0x55535250
USRSPACE_VERSION = 1

# Event type names
EVENT_NAMES = {
    0: "INTF_ADD",
    1: "INTF_DELETE",
    2: "INTF_UP",
    3: "INTF_DOWN",
    4: "INTF_ADDR_ADD",
    5: "INTF_ADDR_DEL",
    6: "MAC_ADD",
    7: "MAC_DEL",
    8: "NEIGH_ADD",
    9: "NEIGH_UPDATE",
    10: "NEIGH_DEL",
}

def create_listener():
    """Create Unix domain socket listener"""
    # Remove old socket if exists
    if os.path.exists(SOCKET_PATH):
        os.unlink(SOCKET_PATH)

    # Create socket
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    sock.bind(SOCKET_PATH)
    os.chmod(SOCKET_PATH, 0o666)

    print(f"✓ Socket listener created at {SOCKET_PATH}")
    print("✓ Waiting for events from userspace provider...")
    print()

    return sock

def process_event(data):
    """Process received event"""
    if len(data) < 16:
        print(f"✗ Message too short: {len(data)} bytes")
        return

    # Parse header
    magic, version, event_type, ns_id = struct.unpack("IIII", data[:16])

    # Validate
    if magic != USRSPACE_MAGIC:
        print(f"✗ Invalid magic: 0x{magic:08x}")
        return

    if version != USRSPACE_VERSION:
        print(f"✗ Invalid version: {version}")
        return

    event_name = EVENT_NAMES.get(event_type, f"UNKNOWN({event_type})")

    print(f"✓ Received Event:")
    print(f"  Type:      {event_name} ({event_type})")
    print(f"  NS ID:     {ns_id}")
    print(f"  Size:      {len(data)} bytes")
    print(f"  Magic:     0x{magic:08x} ✓")
    print(f"  Version:   {version} ✓")

    # Parse event-specific data
    if event_type in [0, 1, 2, 3]:  # Interface events
        # Try to parse interface name (first 16 bytes of data)
        try:
            ifname_bytes = data[16:32]
            ifname = ifname_bytes.split(b'\x00')[0].decode('utf-8')
            if ifname:
                print(f"  Interface: {ifname}")
        except:
            pass

    elif event_type == 6:  # MAC_ADD
        try:
            # Skip to MAC address (rough approximation)
            # In real implementation this would parse full structure
            print(f"  Event:     MAC addition")
        except:
            pass

    print()
    return True

def main():
    print("="*60)
    print("ZEBRA USERSPACE PROVIDER - MINIMAL TEST LISTENER")
    print("="*60)
    print()
    print("This simulates zebra's mock provider socket listener")
    print("without requiring full FRR build.")
    print()

    sock = create_listener()

    events_received = 0

    try:
        print("Press Ctrl+C to stop...")
        print()

        while True:
            data, addr = sock.recvfrom(8192)
            if process_event(data):
                events_received += 1
                print(f"Total events received: {events_received}")
                print("-"*60)
                print()

    except KeyboardInterrupt:
        print()
        print("="*60)
        print(f"✓ Stopped. Total events received: {events_received}")
        print("="*60)
    finally:
        sock.close()
        if os.path.exists(SOCKET_PATH):
            os.unlink(SOCKET_PATH)

if __name__ == "__main__":
    main()
