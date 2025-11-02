# OVS Backend for Zebra EVPN - Implementation Guide

## Overview

This guide provides the minimal implementation for OVS backend support in Zebra,
focusing on Phase 1 (Foundation) and Phase 3 (MAC/Neighbor) only.

NH/NHG, DF election, and SPH filtering are stubbed out.

## Build System Changes

### 1. configure.ac (add after line 1360, after `AM_CONDITIONAL([LINUX]...)`):

```bash
dnl ------------------------
dnl OVS backend support
dnl ------------------------
AC_ARG_ENABLE([ovs],
  AS_HELP_STRING([--enable-ovs], [Enable OVS backend for EVPN (experimental)]))

have_ovs=no
if test "$enable_ovs" = "yes"; then
  # OVS requires Linux (for now)
  if test "${is_linux}" != "true"; then
    AC_MSG_ERROR([OVS backend currently requires Linux])
  fi

  # Check for OVS development libraries
  PKG_CHECK_MODULES([OVS], [libopenvswitch >= 2.13], [
    have_ovs=yes
    AC_DEFINE([HAVE_OVS], [1], [OVS backend])
    # Disable netlink when OVS is enabled (mutually exclusive)
    AC_DEFINE([HAVE_NETLINK], [0], [netlink disabled for OVS])
  ], [
    AC_MSG_ERROR([libopenvswitch >= 2.13 not found. Install openvswitch-dev or openvswitch-devel])
  ])
fi

AM_CONDITIONAL([HAVE_OVS], [test "$have_ovs" = "yes"])
```

### 2. zebra/subdir.am modifications:

Add after line 118 (after `zebra/zserv.c`):

```makefile
# OVS backend files (conditional)
if HAVE_OVS
zebra_zebra_SOURCES += \
	zebra/kernel_ovs.c \
	zebra/rt_ovs.c \
	zebra/ovs_fdb.c \
	zebra/if_ovs.c \
	# end

zebra_zebra_LDADD += $(OVS_LIBS)
endif
```

Add to noinst_HEADERS (after line 150):

```makefile
if HAVE_OVS
noinst_HEADERS += \
	zebra/kernel_ovs.h \
	# end
endif
```

## File Structure

```
zebra/
├── kernel_ovs.h       [NEW] - OVS backend header
├── kernel_ovs.c       [NEW] - OVS initialization and event loop
├── rt_ovs.c           [NEW] - Route/NH/NHG operations (stubbed)
├── ovs_fdb.c          [NEW] - MAC/Neighbor FDB operations
├── if_ovs.c           [NEW] - Interface discovery (stubbed)
└── zebra_ns.h         [MODIFIED] - Add OVS connection structure
```

## Compilation

```bash
# Install OVS development libraries (Ubuntu/Debian)
sudo apt-get install libopenvswitch-dev

# Or on RHEL/CentOS
sudo yum install openvswitch-devel

# Configure FRR with OVS backend
./bootstrap.sh
./configure --enable-ovs --enable-dev-build
make
sudo make install
```

## Configuration

### OVS Setup

```bash
# Create OVS bridge
ovs-vsctl add-br br-int

# Add VXLAN port
ovs-vsctl add-port br-int vxlan0 -- \
  set interface vxlan0 type=vxlan \
  options:remote_ip=flow \
  options:key=flow

# Verify
ovs-vsctl show
```

### Zebra Configuration

```
# /etc/frr/zebra.conf
!
interface br-int
 description OVS Integration Bridge
!
interface vxlan0
 description VXLAN Tunnel Interface
!
```

## Testing

### Verify OVS Backend Initialization

```bash
# Start zebra
sudo /usr/lib/frr/zebra

# Check logs
sudo tail -f /var/log/frr/zebra.log | grep OVS
# Should see: "OVS backend initialized for bridge br-int"
```

### Test Remote MAC Programming

```bash
# From BGP or manual ZAPI, trigger remote MAC add
# Then verify in OVSDB:
ovs-vsctl list Ucast_Macs_Remote

# Should show the programmed MACs
```

### Test Neighbor Programming

```bash
# Trigger remote neighbor add
# Verify OpenFlow flows:
ovs-ofctl dump-flows br-int table=20

# Should show ARP responder flows
```

## Limitations (By Design)

1. **No NH/NHG support**: All remote MACs use direct VTEP IP, no ES failover
2. **No DF election**: No DF/non-DF enforcement via flows
3. **No SPH filtering**: Source port hash filtering not implemented
4. **No backup NHG**: No fast failover on access port down
5. **Limited interface discovery**: Assumes interfaces pre-configured

These limitations mean:
- EVPN MH (multihoming) **will not work** - only single-homed EVPN
- All EVPN logic runs normally, but failover is handled by BGP/control plane
- Suitable for simple VXLAN overlays without multihoming requirements

## Next Steps (Future Work)

To enable full EVPN MH support, implement:

1. **Phase 2 (NH/NHG)**: OpenFlow group creation for ES failover
2. **Phase 4 (DF/SPH)**: OpenFlow flows for DF election enforcement
3. **Phase 5 (Interface Discovery)**: OVSDB monitoring for dynamic interfaces

## Troubleshooting

### "libopenvswitch not found"

```bash
# Verify OVS installation
pkg-config --modversion libopenvswitch

# If not found, install development package
sudo apt-get install libopenvswitch-dev
```

### "Cannot connect to OVSDB"

```bash
# Check OVSDB is running
sudo systemctl status ovsdb-server

# Check socket permissions
ls -la /var/run/openvswitch/db.sock

# Start if needed
sudo systemctl start openvswitch-switch
```

### "OpenFlow connection failed"

```bash
# Verify bridge has OpenFlow controller disabled
# (We connect directly as client, not controller)
ovs-vsctl get-controller br-int
# Should be empty

# If set, remove:
ovs-vsctl del-controller br-int
```

## Code Files (Below)

See the following implementation files:
- zebra/kernel_ovs.h
- zebra/kernel_ovs.c
- zebra/rt_ovs.c
- zebra/ovs_fdb.c
- zebra/if_ovs.c
- zebra/zebra_ns.h (modifications)
