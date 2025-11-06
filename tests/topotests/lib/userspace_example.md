# FRR Userspace Provider for Topotests

This document explains how to use the userspace provider to run FRR topotests without kernel dependencies.

## Overview

The userspace provider allows zebra to receive interface, MAC/FDB, and neighbor/ARP events from userspace instead of the Linux kernel. This enables:

1. Running tests without requiring real network interfaces
2. Testing EVPN multihoming scenarios without kernel support
3. Faster test execution by bypassing kernel operations
4. More deterministic test behavior

## Architecture

```
┌─────────────────────────────────────────┐
│          Topotest (Python)              │
│                                         │
│  UsrspaceProvider.inject_intf_add()    │
│  UsrspaceProvider.inject_mac_add()     │
│  UsrspaceProvider.inject_neigh_add()   │
└────────────────┬────────────────────────┘
                 │ Unix Socket
                 │ (/var/run/frr/zebra_usrspace.sock)
                 │
┌────────────────▼────────────────────────┐
│          Zebra (FRR)                    │
│                                         │
│  ┌──────────────────────────────────┐  │
│  │  Userspace Mock Provider         │  │
│  │  - Receives events via socket    │  │
│  │  - Injects into zebra core       │  │
│  └──────────────┬───────────────────┘  │
│                 │                       │
│  ┌──────────────▼───────────────────┐  │
│  │  Zebra Core                       │  │
│  │  - Interface management           │  │
│  │  - EVPN multihoming               │  │
│  │  - MAC/FDB learning               │  │
│  │  - Neighbor management            │  │
│  └───────────────────────────────────┘  │
└─────────────────────────────────────────┘
```

## Usage in Topotests

### 1. Enable Userspace Mode

```python
from lib import usrspace_provider

def setup_module(module):
    tgen = Topogen(build_topo, module.__name__)
    tgen.start_topology()

    # Load configs as usual
    router_list = tgen.routers()
    for rname, router in router_list.items():
        router.load_config(...)

    # Enable userspace mode on all routers
    for rname, router in router_list.items():
        usrspace_provider.enable_userspace_mode(router)

    tgen.start_router()
```

### 2. Create Userspace Provider Client

```python
# Create provider client for injecting events
provider = usrspace_provider.UsrspaceProvider()
provider.connect()
```

### 3. Inject Events

#### Interface Events

```python
# Add interface
provider.inject_intf_add(
    ifname="eth0",
    ifindex=10,
    mtu=1500,
    hw_addr=b'\x00\x11\x22\x33\x44\x55'
)

# Bring interface up
provider.inject_intf_up(ifindex=10)

# Add IP address
provider.inject_intf_addr_add(
    ifindex=10,
    addr="192.168.1.1",
    prefixlen=24
)

# Bring interface down
provider.inject_intf_down(ifindex=10)

# Delete interface
provider.inject_intf_delete(ifindex=10)
```

#### VXLAN Interface Example

```python
# Add VXLAN interface
provider.inject_intf_add(
    ifname="vxlan1000",
    ifindex=100,
    mtu=9152,
    is_vxlan=True,
    vni=1000,
    vtep_ip="192.168.100.1"
)
provider.inject_intf_up(ifindex=100)
```

#### Bridge Interface Example

```python
# Add bridge
provider.inject_intf_add(
    ifname="br0",
    ifindex=50,
    mtu=1500,
    is_bridge=True
)
provider.inject_intf_up(ifindex=50)
```

#### MAC/FDB Events

```python
# Add local MAC
provider.inject_mac_add(
    ifindex=10,
    vni=1000,
    mac=b'\xaa\xbb\xcc\xdd\xee\xff',
    vid=100,
    is_local=True
)

# Add remote MAC with VTEP
provider.inject_mac_add(
    ifindex=100,  # VXLAN interface
    vni=1000,
    mac=b'\x11\x22\x33\x44\x55\x66',
    vid=100,
    is_local=False,
    vtep_ip="192.168.100.2"
)

# Add MAC with ESI for EVPN multihoming
esi = b'\x03\x44\x38\x39\xff\xff\x01\x00\x00\x01'  # Type 3 ESI
provider.inject_mac_add(
    ifindex=10,
    vni=1000,
    mac=b'\xaa\xbb\xcc\xdd\xee\xff',
    vid=100,
    is_local=True,
    esi=esi
)

# Delete MAC
provider.inject_mac_del(
    ifindex=10,
    vni=1000,
    mac=b'\xaa\xbb\xcc\xdd\xee\xff',
    vid=100
)
```

#### Neighbor/ARP Events

```python
# Add IPv4 neighbor
provider.inject_neigh_add(
    ifindex=10,
    ip="192.168.1.100",
    mac=b'\xaa\xbb\xcc\xdd\xee\xff',
    family=socket.AF_INET
)

# Add IPv6 neighbor
provider.inject_neigh_add(
    ifindex=10,
    ip="2001:db8::1",
    mac=b'\xaa\xbb\xcc\xdd\xee\xff',
    family=socket.AF_INET6
)

# Delete neighbor
provider.inject_neigh_del(
    ifindex=10,
    ip="192.168.1.100",
    family=socket.AF_INET
)
```

### 4. Cleanup

```python
def teardown_module(module):
    provider.close()
    tgen = get_topogen()
    tgen.stop_topology()
```

## EVPN Multihoming Example

Here's a complete example for testing EVPN multihoming:

```python
def test_evpn_mh_userspace():
    tgen = get_topogen()
    provider = usrspace_provider.UsrspaceProvider()
    provider.connect()

    # Create bonds on TOR
    provider.inject_intf_add(
        ifname="hostbond1",
        ifindex=200,
        mtu=1500,
        hw_addr=b'\x44\x38\x39\xff\xff\x01',
        is_bond=True
    )
    provider.inject_intf_up(ifindex=200)

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

    # Verify ES in BGP
    output = tgen.gears["tor1"].vtysh_cmd("show bgp l2vpn evpn es json")
    es_json = json.loads(output)
    # ... verify ES configuration

    provider.close()
```

## Advantages

1. **No Kernel Dependencies**: Tests run without requiring kernel networking features
2. **Speed**: Much faster than creating real interfaces and waiting for kernel events
3. **Determinism**: Precise control over event timing and ordering
4. **Isolation**: No interference from real network interfaces
5. **Portability**: Can run in containers or VMs without special privileges

## Limitations

1. **No Real Traffic**: Cannot test actual packet forwarding
2. **Mock Events Only**: Events must be explicitly injected, no automatic detection
3. **Simplified Model**: Some kernel behaviors may not be perfectly replicated

## Debugging

Enable debug logging in zebra:

```
configure terminal
debug zebra events
debug zebra vxlan
debug zebra evpn mh mac
debug zebra kernel
```

View userspace provider status:

```
show userspace-dataplane provider
```

## Configuration

In zebra.conf:

```
userspace-dataplane
```

Or via vtysh:

```
configure terminal
userspace-dataplane
```

To disable:

```
configure terminal
no userspace-dataplane
```
