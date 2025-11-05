#!/usr/bin/env python
# SPDX-License-Identifier: ISC

"""
Python bindings for FRR userspace event provider.

This module allows topotests to inject interface, MAC/FDB, and neighbor/ARP
events into zebra without requiring kernel dependencies. It communicates with
zebra's mock provider via a Unix domain socket.

Copyright (C) 2025 Free Range Routing
"""

import socket
import struct
import os
import logging

# Event types (must match zebra_usrspace_provider.h)
USRSPACE_EVENT_INTF_ADD = 0
USRSPACE_EVENT_INTF_DELETE = 1
USRSPACE_EVENT_INTF_UP = 2
USRSPACE_EVENT_INTF_DOWN = 3
USRSPACE_EVENT_INTF_ADDR_ADD = 4
USRSPACE_EVENT_INTF_ADDR_DEL = 5
USRSPACE_EVENT_MAC_ADD = 6
USRSPACE_EVENT_MAC_DEL = 7
USRSPACE_EVENT_NEIGH_ADD = 8
USRSPACE_EVENT_NEIGH_UPDATE = 9
USRSPACE_EVENT_NEIGH_DEL = 10
USRSPACE_EVENT_BR_PORT_UPDATE = 11
USRSPACE_EVENT_VLAN_ADD = 12
USRSPACE_EVENT_VLAN_DEL = 13

# Magic and version for wire protocol
USRSPACE_MAGIC = 0x55535250  # "USRP"
USRSPACE_VERSION = 1

# Constants
IFNAMSIZ = 16
ETH_ALEN = 6
VRF_DEFAULT = 0

logger = logging.getLogger(__name__)


class UsrspaceProvider:
    """
    Userspace provider client for injecting events into zebra.
    """

    def __init__(self, sock_path="/var/run/frr/zebra_usrspace.sock"):
        """
        Initialize the userspace provider client.

        Args:
            sock_path: Path to the Unix domain socket
        """
        self.sock_path = sock_path
        self.sock = None

    def connect(self):
        """
        Connect to the zebra userspace provider socket.
        """
        if self.sock:
            return

        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        logger.debug(f"Connected to userspace provider at {self.sock_path}")

    def close(self):
        """
        Close the connection.
        """
        if self.sock:
            self.sock.close()
            self.sock = None

    def _send_event(self, event_type, ns_id, data):
        """
        Send an event to zebra.

        Args:
            event_type: Event type constant
            ns_id: Namespace ID
            data: Binary event data (bytes)
        """
        if not self.sock:
            self.connect()

        # Build wire event header
        wire_hdr = struct.pack(
            "IIII",
            USRSPACE_MAGIC,
            USRSPACE_VERSION,
            event_type,
            ns_id,
        )

        # Pad data to 4096 bytes
        padded_data = data + b'\x00' * (4096 - len(data))

        # Send the message
        msg = wire_hdr + padded_data
        try:
            self.sock.sendto(msg, self.sock_path)
            logger.debug(f"Sent event type {event_type} to {self.sock_path}")
        except Exception as e:
            logger.error(f"Failed to send event: {e}")
            raise

    def inject_intf_add(self, ifname, ifindex, mtu=1500, hw_addr=None, ns_id=0,
                        vrf_id=VRF_DEFAULT, is_bridge=False, is_vxlan=False,
                        vni=0, vtep_ip="0.0.0.0", is_bond=False, is_vlan=False,
                        vlan_id=0):
        """
        Inject interface add event.

        Args:
            ifname: Interface name (string)
            ifindex: Interface index (int)
            mtu: MTU (int)
            hw_addr: Hardware address (bytes or None)
            ns_id: Namespace ID (int)
            vrf_id: VRF ID (int)
            is_bridge: Is this a bridge? (bool)
            is_vxlan: Is this a VXLAN interface? (bool)
            vni: VXLAN VNI (int)
            vtep_ip: VTEP IP address (string)
            is_bond: Is this a bond/LAG? (bool)
            is_vlan: Is this a VLAN interface? (bool)
            vlan_id: VLAN ID (int)
        """
        # Build interface event structure
        ifname_bytes = ifname.encode('utf-8')[:IFNAMSIZ-1] + b'\x00'
        ifname_bytes = ifname_bytes.ljust(IFNAMSIZ, b'\x00')

        hw_addr_bytes = hw_addr if hw_addr else b'\x00' * ETH_ALEN
        hw_addr_len = len(hw_addr) if hw_addr else 0

        # Interface type (simplified)
        zif_type = 1  # ZEBRA_IF_OTHER
        if is_bridge:
            zif_type = 2  # ZEBRA_IF_BRIDGE
        elif is_bond:
            zif_type = 3  # ZEBRA_IF_BOND
        elif is_vxlan:
            zif_type = 4  # ZEBRA_IF_VXLAN

        # Convert VTEP IP to 32-bit integer
        vtep_ip_int = struct.unpack("!I", socket.inet_aton(vtep_ip))[0]

        # Pack the interface event (simplified structure)
        # Format: ifname(16), ifindex(I), mtu(I), flags(I), hw_addr(6), hw_addr_len(I),
        #         zif_type(I), vrf_id(I), bools(4*I), vni(I), vtep_ip(I), vlan_id(H) + padding
        data = struct.pack(
            f"{IFNAMSIZ}sIII{ETH_ALEN}sIIIIIIIIIIH2x",
            ifname_bytes,
            ifindex,
            mtu,
            0x1 | 0x40,  # IFF_UP | IFF_RUNNING
            hw_addr_bytes[:ETH_ALEN],
            hw_addr_len,
            zif_type,
            vrf_id,
            int(is_bond),
            0,  # is_bond_member
            int(is_bridge),
            0,  # is_bridge_member
            int(is_vxlan),
            vni,
            vtep_ip_int,
            int(is_vlan),
            vlan_id,
        )

        self._send_event(USRSPACE_EVENT_INTF_ADD, ns_id, data)

    def inject_intf_delete(self, ifindex, ns_id=0):
        """
        Inject interface delete event.

        Args:
            ifindex: Interface index (int)
            ns_id: Namespace ID (int)
        """
        # Pack minimal interface event with just ifindex
        data = struct.pack(f"{IFNAMSIZ}sIII{ETH_ALEN}s",
                          b'', ifindex, 0, 0, b'')
        self._send_event(USRSPACE_EVENT_INTF_DELETE, ns_id, data)

    def inject_intf_up(self, ifindex, ns_id=0):
        """
        Inject interface up event.

        Args:
            ifindex: Interface index (int)
            ns_id: Namespace ID (int)
        """
        data = struct.pack(f"{IFNAMSIZ}sIII{ETH_ALEN}s",
                          b'', ifindex, 0, 0, b'')
        self._send_event(USRSPACE_EVENT_INTF_UP, ns_id, data)

    def inject_intf_down(self, ifindex, ns_id=0):
        """
        Inject interface down event.

        Args:
            ifindex: Interface index (int)
            ns_id: Namespace ID (int)
        """
        data = struct.pack(f"{IFNAMSIZ}sIII{ETH_ALEN}s",
                          b'', ifindex, 0, 0, b'')
        self._send_event(USRSPACE_EVENT_INTF_DOWN, ns_id, data)

    def inject_intf_addr_add(self, ifindex, addr, prefixlen, family=socket.AF_INET, ns_id=0):
        """
        Inject interface address add event.

        Args:
            ifindex: Interface index (int)
            addr: IP address (string)
            prefixlen: Prefix length (int)
            family: Address family (AF_INET or AF_INET6)
            ns_id: Namespace ID (int)
        """
        # Convert address to bytes
        if family == socket.AF_INET:
            addr_bytes = socket.inet_pton(socket.AF_INET, addr)
            addr_bytes = addr_bytes + b'\x00' * 12  # Pad to 16 bytes
        else:
            addr_bytes = socket.inet_pton(socket.AF_INET6, addr)

        # Pack address event: ifindex(I), family(i), addr(16), prefixlen(B), padding
        data = struct.pack(f"Ii16sB3x", ifindex, family, addr_bytes, prefixlen)
        self._send_event(USRSPACE_EVENT_INTF_ADDR_ADD, ns_id, data)

    def inject_intf_addr_del(self, ifindex, addr, prefixlen, family=socket.AF_INET, ns_id=0):
        """
        Inject interface address delete event.

        Args:
            ifindex: Interface index (int)
            addr: IP address (string)
            prefixlen: Prefix length (int)
            family: Address family (AF_INET or AF_INET6)
            ns_id: Namespace ID (int)
        """
        # Convert address to bytes
        if family == socket.AF_INET:
            addr_bytes = socket.inet_pton(socket.AF_INET, addr)
            addr_bytes = addr_bytes + b'\x00' * 12  # Pad to 16 bytes
        else:
            addr_bytes = socket.inet_pton(socket.AF_INET6, addr)

        # Pack address event
        data = struct.pack(f"Ii16sB3x", ifindex, family, addr_bytes, prefixlen)
        self._send_event(USRSPACE_EVENT_INTF_ADDR_DEL, ns_id, data)

    def inject_mac_add(self, ifindex, vni, mac, vid=0, is_local=True,
                       vtep_ip=None, esi=None, ns_id=0):
        """
        Inject MAC/FDB add event.

        Args:
            ifindex: Interface index (int)
            vni: VXLAN VNI (int)
            mac: MAC address (bytes, 6 bytes)
            vid: VLAN ID (int)
            is_local: Is this a local MAC? (bool)
            vtep_ip: Remote VTEP IP for remote MACs (string or None)
            esi: Ethernet Segment ID (bytes, 10 bytes, or None)
            ns_id: Namespace ID (int)
        """
        # Convert VTEP IP if provided
        if vtep_ip and not is_local:
            vtep_ip_bytes = socket.inet_aton(vtep_ip)
            vtep_family = socket.AF_INET
        else:
            vtep_ip_bytes = b'\x00' * 4
            vtep_family = 0

        # ESI
        if esi:
            esi_bytes = esi[:10]
            esi_valid = 1
        else:
            esi_bytes = b'\x00' * 10
            esi_valid = 0

        # Pack MAC event: ifindex(I), vni(I), mac(6), 2x pad, vtep(4+12), vid(H),
        #                 flags(4*I), esi(10), esi_valid(I)
        data = struct.pack(
            f"II6s2x4x12xH6xIIII10sI",
            ifindex,
            vni,
            mac[:6],
            vid,
            int(is_local),
            0,  # is_static
            0,  # is_sticky
            0,  # is_router
            esi_bytes,
            esi_valid,
        )
        self._send_event(USRSPACE_EVENT_MAC_ADD, ns_id, data)

    def inject_mac_del(self, ifindex, vni, mac, vid=0, ns_id=0):
        """
        Inject MAC/FDB delete event.

        Args:
            ifindex: Interface index (int)
            vni: VXLAN VNI (int)
            mac: MAC address (bytes, 6 bytes)
            vid: VLAN ID (int)
            ns_id: Namespace ID (int)
        """
        # Pack MAC event
        data = struct.pack(f"II6s2xH", ifindex, vni, mac[:6], vid)
        self._send_event(USRSPACE_EVENT_MAC_DEL, ns_id, data)

    def inject_neigh_add(self, ifindex, ip, mac, family=socket.AF_INET, ns_id=0):
        """
        Inject neighbor/ARP add event.

        Args:
            ifindex: Interface index (int)
            ip: IP address (string)
            mac: MAC address (bytes, 6 bytes)
            family: Address family (AF_INET or AF_INET6)
            ns_id: Namespace ID (int)
        """
        # Convert IP to bytes
        if family == socket.AF_INET:
            ip_bytes = socket.inet_pton(socket.AF_INET, ip)
            ip_bytes = ip_bytes + b'\x00' * 12  # Pad to 16 bytes
        else:
            ip_bytes = socket.inet_pton(socket.AF_INET6, ip)

        # Pack neighbor event: ifindex(I), family(i), ip(16), mac(6), state(H), flags(I)
        data = struct.pack(
            f"Ii16s6sHI",
            ifindex,
            family,
            ip_bytes,
            mac[:6],
            0x01,  # DPLANE_NUD_REACHABLE
            0,     # flags
        )
        self._send_event(USRSPACE_EVENT_NEIGH_ADD, ns_id, data)

    def inject_neigh_del(self, ifindex, ip, family=socket.AF_INET, ns_id=0):
        """
        Inject neighbor/ARP delete event.

        Args:
            ifindex: Interface index (int)
            ip: IP address (string)
            family: Address family (AF_INET or AF_INET6)
            ns_id: Namespace ID (int)
        """
        # Convert IP to bytes
        if family == socket.AF_INET:
            ip_bytes = socket.inet_pton(socket.AF_INET, ip)
            ip_bytes = ip_bytes + b'\x00' * 12
        else:
            ip_bytes = socket.inet_pton(socket.AF_INET6, ip)

        # Pack neighbor event
        data = struct.pack(f"Ii16s", ifindex, family, ip_bytes)
        self._send_event(USRSPACE_EVENT_NEIGH_DEL, ns_id, data)


def enable_userspace_mode(router):
    """
    Enable userspace mode on a router.

    Args:
        router: Topogen router object
    """
    router.vtysh_cmd("configure terminal\nuserspace-dataplane")
    logger.info(f"Enabled userspace mode on {router.name}")


def disable_userspace_mode(router):
    """
    Disable userspace mode on a router.

    Args:
        router: Topogen router object
    """
    router.vtysh_cmd("configure terminal\nno userspace-dataplane")
    logger.info(f"Disabled userspace mode on {router.name}")
