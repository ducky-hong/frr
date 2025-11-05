.. _userspace-provider:

*************************
Userspace Event Provider
*************************

Overview
========

The userspace event provider allows FRR's zebra daemon to receive interface, MAC/FDB,
and neighbor/ARP events from userspace instead of the Linux kernel. This enables:

- Running FRR without kernel dependencies
- Testing EVPN multihoming and other features in simulated environments
- Integration with non-Linux data planes
- Faster and more deterministic testing

Architecture
============

Traditional Architecture (Kernel Mode)
--------------------------------------

::

   ┌─────────────────────────────────────┐
   │        Zebra Daemon                 │
   │                                     │
   │  ┌──────────────────────────────┐  │
   │  │  Netlink Socket              │  │
   │  │  - RTM_NEWLINK/DELLINK       │  │
   │  │  - RTM_NEWNEIGH/DELNEIGH     │  │
   │  │  - RTM_NEWADDR/DELADDR       │  │
   │  └──────────┬───────────────────┘  │
   └─────────────┼───────────────────────┘
                 │ Netlink
   ──────────────┼────────────────────────
                 │ Kernel Space
   ┌─────────────▼───────────────────────┐
   │      Linux Kernel                   │
   │  - Network interfaces               │
   │  - Bridge FDB                       │
   │  - Neighbor table                   │
   └─────────────────────────────────────┘

New Architecture (Userspace Mode)
----------------------------------

::

   ┌─────────────────────────────────────┐
   │        Zebra Daemon                 │
   │                                     │
   │  ┌──────────────────────────────┐  │
   │  │  Userspace Provider API      │  │
   │  │  - Interface events          │  │
   │  │  - MAC/FDB events            │  │
   │  │  - Neighbor events           │  │
   │  └──────────┬───────────────────┘  │
   └─────────────┼───────────────────────┘
                 │
        ┌────────┴─────────┐
        │                  │
   ┌────▼────┐      ┌──────▼──────┐
   │ Kernel  │      │  Userspace  │
   │(netlink)│      │   Provider  │
   └─────────┘      │  (Mock/Test)│
                    └─────────────┘

Components
==========

Core Components
---------------

1. **zebra_usrspace_provider.h/c**: Core API and event processing
2. **zebra_usrspace_mock.h/c**: Mock provider implementation for testing
3. **tests/lib/usrspace_provider.py**: Python bindings for topotests

Provider API
------------

Event Types
^^^^^^^^^^^

.. c:enum:: usrspace_event_type

   Event types that can be injected from userspace:

   .. c:enumerator:: USRSPACE_EVENT_INTF_ADD

      Interface added

   .. c:enumerator:: USRSPACE_EVENT_INTF_DELETE

      Interface deleted

   .. c:enumerator:: USRSPACE_EVENT_INTF_UP

      Interface brought up

   .. c:enumerator:: USRSPACE_EVENT_INTF_DOWN

      Interface brought down

   .. c:enumerator:: USRSPACE_EVENT_INTF_ADDR_ADD

      Address added to interface

   .. c:enumerator:: USRSPACE_EVENT_INTF_ADDR_DEL

      Address removed from interface

   .. c:enumerator:: USRSPACE_EVENT_MAC_ADD

      MAC address added to FDB

   .. c:enumerator:: USRSPACE_EVENT_MAC_DEL

      MAC address removed from FDB

   .. c:enumerator:: USRSPACE_EVENT_NEIGH_ADD

      Neighbor added

   .. c:enumerator:: USRSPACE_EVENT_NEIGH_UPDATE

      Neighbor updated

   .. c:enumerator:: USRSPACE_EVENT_NEIGH_DEL

      Neighbor deleted

Key Functions
^^^^^^^^^^^^^

.. c:function:: void zebra_usrspace_provider_init(void)

   Initialize the userspace provider subsystem. Called during zebra startup.

.. c:function:: int zebra_usrspace_provider_register(struct usrspace_provider *prov)

   Register a new provider implementation.

.. c:function:: void zebra_usrspace_set_enabled(bool enabled)

   Enable or disable userspace mode.

.. c:function:: int zebra_usrspace_inject_event(struct usrspace_event *event)

   Inject an event into zebra for processing.

Helper Functions
^^^^^^^^^^^^^^^^

.. c:function:: int zebra_usrspace_inject_intf_add(ns_id_t ns_id, const char *ifname, ifindex_t ifindex, uint32_t mtu, const uint8_t *hw_addr)

   Inject an interface add event.

.. c:function:: int zebra_usrspace_inject_intf_up(ns_id_t ns_id, ifindex_t ifindex)

   Inject an interface up event.

.. c:function:: int zebra_usrspace_inject_mac_add(ns_id_t ns_id, ifindex_t ifindex, uint32_t vni, const uint8_t *mac, vlanid_t vid, bool is_local, const struct ipaddr *vtep_ip, const esi_t *esi)

   Inject a MAC address add event with optional ESI for EVPN multihoming.

Mock Provider
-------------

The mock provider implements a Unix domain socket interface for receiving events
from external processes (e.g., test scripts).

Wire Protocol
^^^^^^^^^^^^^

Messages sent to ``/var/run/frr/zebra_usrspace.sock`` use the following format:

.. code-block:: c

   struct usrspace_wire_event {
       uint32_t magic;     /* 0x55535250 ("USRP") */
       uint32_t version;   /* 1 */
       uint32_t type;      /* Event type */
       uint32_t ns_id;     /* Namespace ID */
       uint8_t data[4096]; /* Event-specific data */
   } __attribute__((packed));

Configuration
=============

Enabling Userspace Mode
------------------------

Via zebra.conf::

   userspace-dataplane

Via vtysh::

   configure terminal
   userspace-dataplane

Disabling Userspace Mode
-------------------------

::

   configure terminal
   no userspace-dataplane

Show Commands
-------------

View provider status::

   show userspace-dataplane provider

Output::

   Provider: mock
   Enabled: yes
   Started: yes

   Mock Provider Statistics:
     Socket path: /var/run/frr/zebra_usrspace.sock
     Events received: 150
     Events processed: 150
     Events dropped: 0
     Queue size: 0

Usage in Topotests
==================

Basic Setup
-----------

.. code-block:: python

   from lib import usrspace_provider

   def setup_module(module):
       tgen = Topogen(build_topo, module.__name__)
       tgen.start_topology()

       # Load configurations
       router_list = tgen.routers()
       for rname, router in router_list.items():
           router.load_config(...)

       # Enable userspace mode
       for rname, router in router_list.items():
           usrspace_provider.enable_userspace_mode(router)

       tgen.start_router()

Injecting Events
----------------

.. code-block:: python

   provider = usrspace_provider.UsrspaceProvider()
   provider.connect()

   # Add interface
   provider.inject_intf_add(
       ifname="eth0",
       ifindex=10,
       mtu=1500,
       hw_addr=b'\x00\x11\x22\x33\x44\x55'
   )

   # Bring up
   provider.inject_intf_up(ifindex=10)

   # Add IP address
   provider.inject_intf_addr_add(
       ifindex=10,
       addr="192.168.1.1",
       prefixlen=24
   )

EVPN Multihoming Example
-------------------------

.. code-block:: python

   # Create VXLAN interface
   provider.inject_intf_add(
       ifname="vxlan1000",
       ifindex=100,
       is_vxlan=True,
       vni=1000,
       vtep_ip="192.168.100.1"
   )
   provider.inject_intf_up(ifindex=100)

   # Create bond interface
   provider.inject_intf_add(
       ifname="bond0",
       ifindex=200,
       is_bond=True,
       hw_addr=b'\x44\x38\x39\xff\xff\x01'
   )
   provider.inject_intf_up(ifindex=200)

   # Add MAC with ESI
   esi = b'\x03\x44\x38\x39\xff\xff\x01\x00\x00\x01'
   provider.inject_mac_add(
       ifindex=200,
       vni=1000,
       mac=b'\x00\x00\x00\x00\x00\x11',
       vid=1000,
       is_local=True,
       esi=esi
   )

Implementing a Custom Provider
===============================

Providers implement the ``usrspace_provider_ops`` interface:

.. code-block:: c

   struct usrspace_provider_ops {
       int (*init)(struct usrspace_provider *prov);
       int (*fini)(struct usrspace_provider *prov);
       int (*start)(struct usrspace_provider *prov);
       int (*stop)(struct usrspace_provider *prov);
       int (*get_fd)(struct usrspace_provider *prov);
       int (*poll)(struct usrspace_provider *prov);
   };

Example Implementation
----------------------

.. code-block:: c

   static int my_provider_init(struct usrspace_provider *prov)
   {
       /* Initialize resources */
       return 0;
   }

   static int my_provider_start(struct usrspace_provider *prov)
   {
       /* Start listening for events */
       return 0;
   }

   static int my_provider_poll(struct usrspace_provider *prov)
   {
       struct usrspace_event event;

       /* Read event from your source */
       /* ... */

       /* Inject into zebra */
       zebra_usrspace_inject_event(&event);

       return 0;
   }

   static const struct usrspace_provider_ops my_ops = {
       .init = my_provider_init,
       .start = my_provider_start,
       .poll = my_provider_poll,
       /* ... */
   };

   static struct usrspace_provider my_provider = {
       .name = "my_provider",
       .ops = &my_ops,
   };

   void my_provider_register(void)
   {
       zebra_usrspace_provider_register(&my_provider);
   }

Use Cases
=========

Testing
-------

- EVPN multihoming scenarios without kernel support
- MAC mobility testing
- Designated Forwarder election
- Interface and uplink tracking
- Fast convergence testing

Simulation
----------

- Network simulators can inject events
- Hardware-in-the-loop testing
- Virtual network functions

Alternative Data Planes
------------------------

- DPDK-based forwarding
- P4/programmable switches
- FPGA/ASIC integration
- eBPF data planes

Limitations
===========

1. **No Actual Forwarding**: The userspace provider only injects control plane events.
   Data plane forwarding must be handled separately.

2. **Manual Event Injection**: Events must be explicitly injected. There's no automatic
   detection of interface changes, unlike with kernel netlink.

3. **Simplified Model**: Some kernel behaviors may not be perfectly replicated
   (e.g., automatic neighbor reachability detection).

4. **No Real Packets**: Cannot test actual packet forwarding without integrating with
   a data plane.

Debugging
=========

Enable Debug Logging
--------------------

::

   configure terminal
   debug zebra events
   debug zebra vxlan
   debug zebra evpn mh mac
   debug zebra kernel

View Logs
---------

::

   tail -f /var/log/frr/zebra.log

Common Issues
-------------

**Socket Permission Denied**

Ensure the socket has correct permissions::

   chmod 666 /var/run/frr/zebra_usrspace.sock

**Events Not Processed**

Check that userspace mode is enabled::

   show userspace-dataplane provider

**Queue Full**

Increase queue size or process events more slowly.

Future Enhancements
===================

Planned features:

1. **eBPF Integration**: Use eBPF programs to intercept kernel events
2. **gRPC Interface**: Remote event injection over gRPC
3. **Event Replay**: Record and replay event sequences
4. **Data Plane Integration**: Connect to DPDK, VPP, or other data planes
5. **Multi-Provider Support**: Run multiple providers simultaneously

References
==========

- :ref:`zebra`
- :ref:`evpn`
- :ref:`testing`
