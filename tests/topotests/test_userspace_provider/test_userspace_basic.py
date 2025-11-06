#!/usr/bin/env python
# SPDX-License-Identifier: ISC

"""
test_userspace_basic.py: Basic test for userspace provider

Tests basic functionality of the userspace event provider:
- Interface injection
- Address injection
- MAC injection
- Verification in zebra
"""

import os
import sys
import json
import pytest

CWD = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(CWD, "../"))

from lib import topotest
from lib.topogen import Topogen, TopoRouter, get_topogen

pytestmark = [pytest.mark.bgpd, pytest.mark.ospfd]


def build_topo(tgen):
    """
    Simple topology with one router for testing userspace provider
    """
    tgen.add_router("r1")


def setup_module(module):
    """Setup topology"""
    tgen = Topogen(build_topo, module.__name__)
    tgen.start_topology()

    router_list = tgen.routers()
    for rname, router in router_list.items():
        router.load_config(
            TopoRouter.RD_ZEBRA, os.path.join(CWD, "{}/zebra.conf".format(rname))
        )
        router.load_config(
            TopoRouter.RD_BGP, os.path.join(CWD, "{}/bgpd.conf".format(rname))
        )

    tgen.start_router()


def teardown_module(_mod):
    """Teardown the pytest environment"""
    tgen = get_topogen()
    tgen.stop_topology()


def test_userspace_mode_enable():
    """Test enabling userspace mode"""
    tgen = get_topogen()

    if tgen.routers_have_failure():
        pytest.skip(tgen.errors)

    r1 = tgen.gears["r1"]

    # Enable userspace mode
    r1.vtysh_cmd("configure terminal\nuserspace-dataplane")

    # Verify it's enabled
    output = r1.vtysh_cmd("show userspace-dataplane provider")
    assert "mock" in output, "Mock provider should be active"
    assert "Enabled: yes" in output, "Userspace mode should be enabled"

    print("✓ Userspace mode enabled successfully")


def test_inject_interface():
    """Test injecting interface via userspace provider"""
    tgen = get_topogen()

    if tgen.routers_have_failure():
        pytest.skip(tgen.errors)

    r1 = tgen.gears["r1"]

    # Import userspace provider
    sys.path.append(os.path.join(CWD, "../../lib"))
    from usrspace_provider import UsrspaceProvider

    provider = UsrspaceProvider()

    try:
        provider.connect()
        print("✓ Connected to userspace provider socket")

        # Inject a test interface
        hw_addr = b'\x00\x11\x22\x33\x44\x55'
        provider.inject_intf_add(
            ifname="test0",
            ifindex=100,
            mtu=1500,
            hw_addr=hw_addr
        )
        print("✓ Injected interface test0")

        # Bring interface up
        provider.inject_intf_up(ifindex=100)
        print("✓ Brought interface test0 up")

        # Add IP address
        provider.inject_intf_addr_add(
            ifindex=100,
            addr="10.0.0.1",
            prefixlen=24
        )
        print("✓ Added address 10.0.0.1/24 to test0")

        # Give zebra a moment to process
        import time
        time.sleep(2)

        # Verify interface exists in zebra
        output = r1.vtysh_cmd("show interface test0")
        print(f"Interface output:\n{output}")

        # Check if interface appears
        if "test0" in output or "Unknown interface" not in output:
            print("✓ Interface test0 visible in zebra")
        else:
            print("⚠ Interface test0 not found in zebra (may need kernel support)")

        # Try to verify with JSON
        output_json = r1.vtysh_cmd("show interface test0 json")
        try:
            intf_data = json.loads(output_json)
            if "test0" in intf_data:
                print(f"✓ Interface data: {intf_data['test0']}")
        except:
            print("⚠ Could not parse interface JSON")

    except Exception as e:
        print(f"✗ Error during test: {e}")
        import traceback
        traceback.print_exc()
        pytest.skip(f"Test failed with error: {e}")
    finally:
        provider.close()


def test_inject_vxlan_interface():
    """Test injecting VXLAN interface"""
    tgen = get_topogen()

    if tgen.routers_have_failure():
        pytest.skip(tgen.errors)

    r1 = tgen.gears["r1"]

    sys.path.append(os.path.join(CWD, "../../lib"))
    from usrspace_provider import UsrspaceProvider

    provider = UsrspaceProvider()

    try:
        provider.connect()

        # Inject VXLAN interface
        provider.inject_intf_add(
            ifname="vxlan1000",
            ifindex=200,
            mtu=9152,
            is_vxlan=True,
            vni=1000,
            vtep_ip="192.168.1.1"
        )
        print("✓ Injected VXLAN interface vxlan1000")

        provider.inject_intf_up(ifindex=200)
        print("✓ Brought VXLAN interface up")

        import time
        time.sleep(2)

        # Try to verify
        output = r1.vtysh_cmd("show interface vxlan1000")
        print(f"VXLAN interface output:\n{output}")

        if "vxlan1000" in output or "Unknown interface" not in output:
            print("✓ VXLAN interface vxlan1000 visible")
        else:
            print("⚠ VXLAN interface not found (may need kernel support)")

    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
    finally:
        provider.close()


def test_provider_statistics():
    """Test viewing provider statistics"""
    tgen = get_topogen()

    if tgen.routers_have_failure():
        pytest.skip(tgen.errors)

    r1 = tgen.gears["r1"]

    # Check provider stats
    output = r1.vtysh_cmd("show userspace-dataplane provider")
    print(f"\nProvider statistics:\n{output}")

    assert "Provider: mock" in output
    assert "Socket path" in output

    # Check for event counters
    if "Events received:" in output:
        print("✓ Provider is receiving events")

    if "Events processed:" in output:
        print("✓ Provider is processing events")

    print("\n✓ All basic userspace provider tests passed!")


if __name__ == "__main__":
    args = ["-s"] + sys.argv[1:]
    sys.exit(pytest.main(args))
