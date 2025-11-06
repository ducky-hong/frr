#!/usr/bin/env python3
"""
Test client that sends events to the mock zebra listener.

This demonstrates the userspace provider working end-to-end.
"""

import sys
import os
import time

sys.path.insert(0, "tests/lib")

from usrspace_provider import UsrspaceProvider

def main():
    print("="*60)
    print("USERSPACE PROVIDER - CLIENT TEST")
    print("="*60)
    print()

    # Override socket path to use test path
    provider = UsrspaceProvider(sock_path="/tmp/test_zebra_usrspace.sock")

    try:
        print("1. Connecting to mock zebra listener...")
        provider.connect()
        print("   ✓ Connected to /tmp/test_zebra_usrspace.sock")
        print()

        # Test 1: Interface Add
        print("2. Injecting Interface Add event...")
        hw_addr = b'\x00\x11\x22\x33\x44\x55'
        provider.inject_intf_add(
            ifname="test0",
            ifindex=100,
            mtu=1500,
            hw_addr=hw_addr
        )
        print("   ✓ Sent INTF_ADD for test0")
        time.sleep(0.5)

        # Test 2: Interface Up
        print()
        print("3. Injecting Interface Up event...")
        provider.inject_intf_up(ifindex=100)
        print("   ✓ Sent INTF_UP for ifindex 100")
        time.sleep(0.5)

        # Test 3: Address Add
        print()
        print("4. Injecting Address Add event...")
        provider.inject_intf_addr_add(
            ifindex=100,
            addr="10.0.0.1",
            prefixlen=24
        )
        print("   ✓ Sent INTF_ADDR_ADD for 10.0.0.1/24")
        time.sleep(0.5)

        # Test 4: VXLAN Interface
        print()
        print("5. Injecting VXLAN interface...")
        provider.inject_intf_add(
            ifname="vxlan1000",
            ifindex=200,
            mtu=9152,
            is_vxlan=True,
            vni=1000,
            vtep_ip="192.168.100.1"
        )
        print("   ✓ Sent INTF_ADD for vxlan1000 (VNI 1000)")
        time.sleep(0.5)

        # Test 5: MAC with ESI
        print()
        print("6. Injecting MAC with ESI (EVPN MH)...")
        esi = b'\x03\x44\x38\x39\xff\xff\x01\x00\x00\x01'
        mac = b'\xaa\xbb\xcc\xdd\xee\xff'
        provider.inject_mac_add(
            ifindex=200,
            vni=1000,
            mac=mac,
            vid=1000,
            is_local=True,
            esi=esi
        )
        print("   ✓ Sent MAC_ADD with ESI 03:44:38:39:ff:ff:01:00:00:01")
        time.sleep(0.5)

        # Test 6: Neighbor
        print()
        print("7. Injecting Neighbor Add event...")
        provider.inject_neigh_add(
            ifindex=100,
            ip="10.0.0.2",
            mac=mac
        )
        print("   ✓ Sent NEIGH_ADD for 10.0.0.2")
        time.sleep(0.5)

        print()
        print("="*60)
        print("✓✓✓ ALL EVENTS SENT SUCCESSFULLY!")
        print("="*60)
        print()
        print("Check the listener output to verify events were received.")
        print()

    except Exception as e:
        print(f"✗ Error: {e}")
        print()
        print("Make sure the listener is running:")
        print("  python3 test_socket_listener.py")
        return 1
    finally:
        provider.close()

    return 0

if __name__ == "__main__":
    sys.exit(main())
