Zebra OVS Backend (EVPN-MH Focus)
=================================

Overview
--------
This document describes a minimal-change plan to replace kernel/netlink
dataplane interactions with Open vSwitch (OVS) while keeping zebra's existing
data model intact. The initial focus is EVPN-MH only. The OVS bridges are the
source of truth for FDB (MAC) and ARP/neighbor state, and zebra consumes flow
events to drive EVPN-MH logic.

Goals
-----
- Keep code changes localized to zebra's kernel-facing entry points.
- Treat VRF/namespace as default-only and warn on non-default usage.
- Use OVS flows as the source of truth for MAC/ARP state.
- Provide a lightweight in-memory route/NH "kernel" state for zebra.
- Use ``ovs-ofctl`` for flow read/write and ``ovs-vsctl`` for metadata.

Non-Goals (initial cut)
-----------------------
- Full kernel parity for routes, MPLS, PBR, TC, or netconf.
- A final OVS pipeline definition (flow format is placeholder and expected to
  be adjusted later).

Plan (Minimal-Change Integration)
---------------------------------
1. Add an OVS configuration module and CLI flags:
   ``--ovs-fdb-bridge``, ``--ovs-arp-bridge``, and ``--ovs-poll-interval``.
2. Register an OVS dataplane provider (pre-kernel) to consume dplane updates,
   mark them "skip-kernel", and keep the kernel provider isolated; disable
   ``kernel_init()``/``kernel_terminate()`` when OVS is enabled.
3. Implement interface discovery via ``ovs-vsctl``:
   build ``struct interface`` objects, set ``ifindex`` (from
   ``external_ids:zebra.ifindex`` or ``ofport``), and map bridge membership.
4. Implement flow monitor + polling using ``ovs-ofctl``:
   - Monitor FDB bridge for MAC flows.
   - Monitor ARP bridge for neighbor flows.
   - Poll with ``dump-flows`` at a slower interval to reconcile.
5. Add a lightweight in-memory map for route/NH installs to satisfy zebra
   expectations about kernel acks and idempotency.

Threading and I/O Options
-------------------------
Current zebra threading context (simplified):
- **Main thread** (``zrouter.master``): CLI, ZAPI, timers, kernel read events.
- **Dplane thread** (``zebra_dplane``): processes outbound dataplane updates.

Netlink "read" is currently wired into the main thread:

.. code-block:: c

   /* zebra/kernel_netlink.c */
   event_add_read(zrouter.master, kernel_read, zns, zns->netlink.sock, NULL);

OVS monitor equivalents can be wired the same way:

.. code-block:: c

   /* zebra/ovs.c (conceptual) */
   event_add_read(zrouter.master, ovs_monitor_read, &mon, mon.fd, &mon.t_read);

Options:
- **A: Main-thread monitor** (minimal change)
  - Monitor flows in the main thread, parse events, update zebra state.
  - Add a slow polling timer for reconciliation.
- **B: Dplane-thread monitor**
  - Monitor in dplane thread, hand off events to main.
- **C: Hybrid**
  - Monitor + periodic poll (monitor for low latency, poll for correctness).
- **D: Dedicated threads**
  - Separate threads for monitor and polling; enqueue events to main/dplane.

Recommended for now: **A + C** (main-thread monitor + polling).

Interface Discovery (OVS)
-------------------------
Zebra builds ``struct interface`` objects from OVS ports:

- **Name**: OVS Interface ``name`` (prefix kept for uniqueness).
- **ifindex**: ``external_ids:zebra.ifindex`` if present, else ``ofport``.
- **Flags**: set ``IFF_UP`` and ``IFF_RUNNING`` based on admin/link state.
- **MTU**: Interface ``mtu`` (or ``external_ids`` fallback).
- **MAC**: Interface ``mac_in_use`` (or ``external_ids`` fallback).
- **VRF**: default only (warn for non-default).

Bridge membership:
- ``B_`` prefix indicates a bridge-equivalent interface.
- ``A_`` prefix indicates a bridge slave.
- Use ``external_ids:zebra.bridge`` to map an ``A_`` port to the ``B_`` bridge.

External IDs (initial keys)
---------------------------
The following keys are expected when OVS metadata is incomplete:

- ``zebra.ifindex``: stable interface index (integer).
- ``zebra.bridge``: bridge name or ifindex for ``A_`` ports.
- ``zebra.mtu`` / ``zebra.mtu6``: MTU overrides.
- ``zebra.mac``: MAC override.
- ``zebra.admin_state`` / ``zebra.link_state``: state overrides.
- ``zebra.desc``: interface description.

EVPN-MH and VxLAN metadata (OVS-specific):

- ``zebra.esi``: Ethernet Segment Identifier (type-0 string) for a bond-
  equivalent access port. Presence implies bond semantics.
- ``zebra.bond``: bond master name or ifindex for a bond member (access port).
- ``zebra.vni``: VNI for a dummy VxLAN interface (per-VNI model).
- ``zebra.vtep_ip``: local VTEP IP for the dummy VxLAN interface.
- ``zebra.mcast_grp``: optional multicast group for VNI.
- ``zebra.access_vlan``: VLAN id used to bind access BD; must match between
  the bond (ES) and VxLAN interface.

Flow Format (Placeholder)
-------------------------
The initial flow format is intentionally simple and should be adjusted later.

EVPN-MH VxLAN Stub
------------------
EVPN-MH logic in zebra expects a VxLAN interface to exist. For OVS this can be
a dummy ``type=vxlan`` port (not used for packet forwarding) with the
``zebra.vni`` and ``zebra.vtep_ip`` external IDs populated.

FDB (MAC) flow example:

.. code-block:: text

   table=0,priority=200,dl_dst=aa:bb:cc:dd:ee:ff,actions=output:7

ARP (neighbor) flow example:

.. code-block:: text

   table=0,priority=200,arp,arp_tpa=192.0.2.1,actions=output:7

Zebra currently parses ``dl_dst`` and/or ``arp_tpa`` plus ``in_port`` where
available, and maps ``in_port`` to an OVS port via ``ofport``.

Default-Only VRF/Namespace
--------------------------
OVS has no VRF/namespace concept. Zebra continues to use the default VRF and
default namespace. Any non-default VRF/NS activation is allowed but logged with
a warning.
