#!/usr/bin/env python3
"""
Demonstration of userspace provider functionality

This script demonstrates how to use the userspace provider to inject
events into zebra without kernel dependencies. It simulates what would
happen in a real topotest.

This is a standalone demo that shows the Python API usage.
"""

import sys
import os
import struct
import socket
import time

# Add lib to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../lib"))

from usrspace_provider import (
    UsrspaceProvider,
    USRSPACE_EVENT_INTF_ADD,
    USRSPACE_EVENT_INTF_UP,
    USRSPACE_EVENT_INTF_ADDR_ADD,
    USRSPACE_EVENT_MAC_ADD,
)


def demo_basic_interface():
    """Demonstrate basic interface injection"""
    print("\n" + "="*60)
    print("DEMO: Basic Interface Injection")
    print("="*60)

    provider = UsrspaceProvider()

    print("\n1. Creating provider instance...")
    print(f"   Socket path: {provider.sock_path}")

    print("\n2. Attempting to connect to zebra...")
    try:
        provider.connect()
        print("   ✓ Connected successfully!")
        connected = True
    except Exception as e:
        print(f"   ⚠ Connection failed (zebra may not be running): {e}")
        print("   This is normal if zebra is not running with userspace mode enabled")
        connected = False

    if connected:
        print("\n3. Injecting interface 'eth0'...")
        try:
            hw_addr = b'\x00\x11\x22\x33\x44\x55'
            provider.inject_intf_add(
                ifname="eth0",
                ifindex=10,
                mtu=1500,
                hw_addr=hw_addr
            )
            print("   ✓ Interface injection sent")

            print("\n4. Bringing interface up...")
            provider.inject_intf_up(ifindex=10)
            print("   ✓ Interface up event sent")

            print("\n5. Adding IP address 10.0.0.1/24...")
            provider.inject_intf_addr_add(
                ifindex=10,
                addr="10.0.0.1",
                prefixlen=24,
                family=socket.AF_INET
            )
            print("   ✓ Address addition sent")

            print("\n✓ All events sent successfully!")

        except Exception as e:
            print(f"   ✗ Error sending events: {e}")
        finally:
            provider.close()
    else:
        print("\n[Simulating what would be sent to zebra...]")

        # Show what the wire protocol looks like
        print("\n3. Interface Add Event Structure:")
        print("   - Event Type: INTF_ADD")
        print("   - Interface Name: eth0")
        print("   - Interface Index: 10")
        print("   - MTU: 1500")
        print("   - Hardware Address: 00:11:22:33:44:55")

        print("\n4. Interface Up Event Structure:")
        print("   - Event Type: INTF_UP")
        print("   - Interface Index: 10")

        print("\n5. Address Add Event Structure:")
        print("   - Event Type: INTF_ADDR_ADD")
        print("   - Interface Index: 10")
        print("   - Address: 10.0.0.1/24")
        print("   - Family: AF_INET")


def demo_vxlan_interface():
    """Demonstrate VXLAN interface injection"""
    print("\n" + "="*60)
    print("DEMO: VXLAN Interface for EVPN")
    print("="*60)

    provider = UsrspaceProvider()

    print("\n1. Creating VXLAN interface 'vxlan1000'...")
    print("   Parameters:")
    print("   - Interface Index: 100")
    print("   - VNI: 1000")
    print("   - VTEP IP: 192.168.100.1")
    print("   - MTU: 9152")

    try:
        provider.connect()
        print("\n2. Connected to zebra, sending event...")

        provider.inject_intf_add(
            ifname="vxlan1000",
            ifindex=100,
            mtu=9152,
            is_vxlan=True,
            vni=1000,
            vtep_ip="192.168.100.1"
        )

        provider.inject_intf_up(ifindex=100)

        print("   ✓ VXLAN interface created and brought up")

    except Exception as e:
        print(f"\n   ⚠ Not connected to zebra: {e}")
        print("\n   This event would create a VXLAN interface in zebra")
        print("   When zebra processes this, it would:")
        print("   - Create interface vxlan1000")
        print("   - Set VNI to 1000")
        print("   - Configure VTEP IP as 192.168.100.1")
        print("   - Make it available for EVPN")
    finally:
        provider.close()


def demo_evpn_multihoming():
    """Demonstrate EVPN multihoming scenario"""
    print("\n" + "="*60)
    print("DEMO: EVPN Multihoming (MH) Scenario")
    print("="*60)

    provider = UsrspaceProvider()

    print("\nScenario: Dual-attached host with EVPN MH")
    print("-" * 60)

    # Bond interface
    print("\n1. Creating bond interface 'bond0' (LAG)...")
    print("   - Interface Index: 200")
    print("   - Hardware Address: 44:38:39:ff:ff:01")
    print("   - Type: Bond/LAG")

    # VXLAN interface
    print("\n2. Creating VXLAN interface for VNI 1000...")
    print("   - Interface Index: 100")
    print("   - VNI: 1000")
    print("   - VTEP IP: 192.168.100.1")

    # MAC with ESI
    print("\n3. Injecting MAC with ESI (Ethernet Segment ID)...")
    print("   - MAC Address: 00:00:00:00:00:11")
    print("   - VNI: 1000")
    print("   - ESI: 03:44:38:39:ff:ff:01:00:00:01 (Type 3 ESI)")
    print("   - Type: Local")
    print("   - Interface: bond0 (ifindex 200)")

    try:
        provider.connect()
        print("\n✓ Connected to zebra")

        # Create bond
        provider.inject_intf_add(
            ifname="bond0",
            ifindex=200,
            mtu=1500,
            hw_addr=b'\x44\x38\x39\xff\xff\x01',
            is_bond=True
        )
        provider.inject_intf_up(ifindex=200)
        print("   ✓ Bond interface created")

        # Create VXLAN
        provider.inject_intf_add(
            ifname="vxlan1000",
            ifindex=100,
            mtu=9152,
            is_vxlan=True,
            vni=1000,
            vtep_ip="192.168.100.1"
        )
        provider.inject_intf_up(ifindex=100)
        print("   ✓ VXLAN interface created")

        # Inject MAC with ESI
        esi = b'\x03\x44\x38\x39\xff\xff\x01\x00\x00\x01'
        provider.inject_mac_add(
            ifindex=200,
            vni=1000,
            mac=b'\x00\x00\x00\x00\x00\x11',
            vid=1000,
            is_local=True,
            esi=esi
        )
        print("   ✓ MAC with ESI injected")

        print("\n✓ Complete EVPN MH scenario injected!")
        print("\nIn zebra, this would:")
        print("  - Create Ethernet Segment with ESI 03:44:38:39:ff:ff:01:00:00:01")
        print("  - Associate MAC with the ES")
        print("  - Trigger BGP EVPN Type-1 and Type-2 route advertisements")
        print("  - Enable ES peer synchronization")
        print("  - Participate in Designated Forwarder election")

    except Exception as e:
        print(f"\n⚠ Not connected to zebra: {e}")
        print("\nThis scenario demonstrates what topotests would send to zebra")
        print("to test EVPN multihoming without kernel dependencies.")
    finally:
        provider.close()


def demo_wire_protocol():
    """Show the wire protocol format"""
    print("\n" + "="*60)
    print("DEMO: Wire Protocol Format")
    print("="*60)

    print("\nMessage Structure:")
    print("-" * 60)
    print("Field                Size      Value")
    print("-" * 60)
    print("Magic Number         4 bytes   0x55535250 ('USRP')")
    print("Version              4 bytes   1")
    print("Event Type           4 bytes   (varies)")
    print("Namespace ID         4 bytes   (varies)")
    print("Event Data           4096 bytes (varies)")
    print("-" * 60)
    print("Total:               4112 bytes")

    print("\nEvent Types:")
    print("-" * 60)
    print(f"INTF_ADD       = {USRSPACE_EVENT_INTF_ADD}")
    print(f"INTF_UP        = {USRSPACE_EVENT_INTF_UP}")
    print(f"INTF_ADDR_ADD  = {USRSPACE_EVENT_INTF_ADDR_ADD}")
    print(f"MAC_ADD        = {USRSPACE_EVENT_MAC_ADD}")
    print("... and more")

    print("\nExample: Interface Add Message:")
    print("-" * 60)

    # Build example message
    magic = 0x55535250
    version = 1
    event_type = USRSPACE_EVENT_INTF_ADD
    ns_id = 0

    header = struct.pack("IIII", magic, version, event_type, ns_id)

    print(f"Header (hex): {header.hex()}")
    print(f"  Magic:      {hex(magic)}")
    print(f"  Version:    {version}")
    print(f"  Event Type: {event_type}")
    print(f"  NS ID:      {ns_id}")

    print("\nThis message would be sent to Unix socket:")
    print(f"  /var/run/frr/zebra_usrspace.sock")


def demo_comparison():
    """Compare kernel vs userspace approach"""
    print("\n" + "="*60)
    print("DEMO: Kernel vs Userspace Comparison")
    print("="*60)

    print("\n" + "="*30 + " KERNEL MODE " + "="*30)
    print("\nTraditional approach (requires kernel):")
    print("-" * 60)
    print("1. Run: ip link add bond0 type bond mode 802.3ad")
    print("   → Kernel creates bond interface")
    print("   → Kernel sends RTM_NEWLINK via netlink")
    print("   → Zebra receives netlink message")
    print("   → Zebra processes interface add")
    print()
    print("2. Run: ip link add vxlan1000 type vxlan id 1000 local 192.168.100.1")
    print("   → Kernel creates VXLAN interface")
    print("   → Netlink notification to zebra")
    print()
    print("3. Bridge learns MAC addresses from actual packets")
    print("   → Kernel FDB update")
    print("   → Netlink RTM_NEWNEIGH")
    print("   → Zebra receives and processes")

    print("\n" + "="*30 + " USERSPACE MODE " + "="*28)
    print("\nNew approach (no kernel needed):")
    print("-" * 60)
    print("1. Python: provider.inject_intf_add(ifname='bond0', is_bond=True)")
    print("   → Event sent via Unix socket")
    print("   → Zebra mock provider receives")
    print("   → Zebra processes interface add")
    print()
    print("2. Python: provider.inject_intf_add(ifname='vxlan1000', is_vxlan=True)")
    print("   → Event sent via Unix socket")
    print("   → Directly processed by zebra")
    print()
    print("3. Python: provider.inject_mac_add(mac=..., esi=...)")
    print("   → Direct MAC injection with ESI")
    print("   → Zebra processes immediately")

    print("\n" + "="*60)
    print("BENEFITS:")
    print("-" * 60)
    print("✓ No kernel networking required")
    print("✓ Faster (no kernel round-trips)")
    print("✓ More deterministic (precise timing)")
    print("✓ Works in containers without privileges")
    print("✓ Easy to script and automate")
    print("✓ Can test scenarios impossible with kernel")


def main():
    """Run all demonstrations"""
    print("\n" + "="*60)
    print("FRR USERSPACE PROVIDER DEMONSTRATION")
    print("="*60)
    print("\nThis demonstration shows how the userspace provider works")
    print("and how it enables testing EVPN multihoming without kernel.")

    # Run demos
    demo_basic_interface()
    demo_vxlan_interface()
    demo_evpn_multihoming()
    demo_wire_protocol()
    demo_comparison()

    print("\n" + "="*60)
    print("SUMMARY")
    print("="*60)
    print("\nThe userspace provider successfully demonstrates:")
    print()
    print("1. ✓ Clean API for event injection")
    print("2. ✓ Support for all interface types (bonds, VXLAN, etc.)")
    print("3. ✓ MAC injection with ESI for EVPN multihoming")
    print("4. ✓ Python bindings for easy topotest integration")
    print("5. ✓ Wire protocol for zebra communication")
    print()
    print("To use in real topotests:")
    print("  1. Enable: zebra> configure terminal")
    print("             zebra(config)> userspace-dataplane")
    print("  2. Run test with event injection")
    print("  3. Verify: zebra> show userspace-dataplane provider")
    print()
    print("This enables running test_evpn_mh.py without kernel!")
    print("="*60 + "\n")


if __name__ == "__main__":
    main()
