// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Zebra OVS backend - Interface discovery
 * Copyright (C) 2025 FRRouting
 */

#include <zebra.h>

#ifdef HAVE_OVS

#include "log.h"
#include "if.h"
#include "prefix.h"

#include "zebra/zebra_ns.h"
#include "zebra/zebra_vrf.h"
#include "zebra/interface.h"
#include "zebra/kernel_ovs.h"
#include "zebra/zebra_dplane.h"
#include "zebra/rt.h"

/*
 * STUB: Interface discovery from OVSDB (Phase 5 - not implemented)
 *
 * In full implementation, this would:
 * 1. Query OVSDB Interface table
 * 2. Create zebra interface structures
 * 3. Link them to the namespace
 */
void interface_list(struct zebra_ns *zns)
{
	zlog_info("OVS: STUB - interface_list() for ns_id %u", zns->ns_id);

	/*
	 * Real implementation would:
	 *
	 * struct ovs_ctx *ctx = zns->ovs_ctx;
	 * const struct ovsrec_interface *iface_row;
	 *
	 * OVSREC_INTERFACE_FOR_EACH(iface_row, ctx->ovs_idl) {
	 *     struct interface *ifp;
	 *
	 *     // Get or create interface
	 *     ifp = if_get_by_name(iface_row->name,
	 *                          VRF_DEFAULT,
	 *                          VRF_DEFAULT_NAME);
	 *
	 *     // Extract ifindex from external_ids
	 *     const char *ifindex_str = smap_get(&iface_row->external_ids,
	 *                                        "ifindex");
	 *     if (ifindex_str)
	 *         ifp->ifindex = atoi(ifindex_str);
	 *
	 *     // Set interface type based on OVS type
	 *     if (strcmp(iface_row->type, "vxlan") == 0) {
	 *         zebra_if_set_ziftype(ifp, ZEBRA_IF_VXLAN,
	 *                             ZEBRA_IF_OTHER);
	 *     } else if (strcmp(iface_row->type, "internal") == 0) {
	 *         zebra_if_set_ziftype(ifp, ZEBRA_IF_BRIDGE,
	 *                             ZEBRA_IF_OTHER);
	 *     }
	 *
	 *     // Extract OVS-specific info
	 *     // - ofport: OpenFlow port number
	 *     // - mac_in_use: Hardware address
	 *     // - admin_state: Up/down state
	 *
	 *     // Mark as active
	 *     if_add_update(ifp);
	 * }
	 */

	/* Notify dataplane that interface read is complete */
	zebra_dplane_startup_stage(zns, ZEBRA_DPLANE_INTERFACES_READ);
}

/*
 * STUB: Second-stage interface initialization
 */
void interface_list_second(struct zebra_ns *zns)
{
	zlog_info("OVS: STUB - interface_list_second() for ns_id %u",
	          zns->ns_id);

	/*
	 * Real implementation would:
	 * - Read MAC FDB from OVSDB Ucast_Macs_Local table
	 * - Read neighbors from flow tables or OVSDB
	 */

	/* Notify dataplane that address read is complete */
	zebra_dplane_startup_stage(zns, ZEBRA_DPLANE_ADDRESSES_READ);
}

/*
 * STUB: Tunnel interface discovery (for VXLAN)
 */
void interface_list_tunneldump(struct zebra_ns *zns)
{
	zlog_info("OVS: STUB - interface_list_tunneldump() for ns_id %u",
	          zns->ns_id);

	/*
	 * Real implementation would read VXLAN tunnel interfaces from OVSDB
	 */

	/* Notify dataplane that tunnel read is complete */
	zebra_dplane_startup_stage(zns, ZEBRA_DPLANE_TUNNELS_READ);
}

/*
 * STUB: Initialize kernel subsystem for namespace
 */
void kernel_init(struct zebra_ns *zns)
{
	zlog_info("OVS: kernel_init() for ns_id %u", zns->ns_id);

	/* Initialize OVS backend */
	ovs_kernel_init(zns);
}

/*
 * STUB: Terminate kernel subsystem for namespace
 */
void kernel_terminate(struct zebra_ns *zns, bool complete)
{
	zlog_info("OVS: kernel_terminate() for ns_id %u (complete=%d)",
	          zns->ns_id, complete);

	/* Terminate OVS backend */
	ovs_kernel_terminate(zns);
}

/*
 * STUB: Read MAC FDB from OVSDB (Phase 5 - not implemented)
 */
void macfdb_read(struct zebra_ns *zns)
{
	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: STUB - macfdb_read() for ns_id %u",
		           zns->ns_id);

	/* Real implementation would query OVSDB Ucast_Macs_Local table */
}

/*
 * STUB: Read MAC FDB for specific bridge
 */
void macfdb_read_for_bridge(struct zebra_ns *zns, struct interface *ifp,
                           struct interface *br_if, vlanid_t vid)
{
	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: STUB - macfdb_read_for_bridge(%s, vid=%u)",
		           br_if ? br_if->name : "NULL", vid);

	/* Real implementation would filter by bridge and VLAN */
}

/*
 * STUB: Read multicast FDB entry for VNI
 */
void macfdb_read_mcast_entry_for_vni(struct zebra_ns *zns,
                                    struct interface *ifp, vni_t vni)
{
	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: STUB - macfdb_read_mcast_entry_for_vni(vni=%u)",
		           vni);

	/* Real implementation would read BUM entry from OVSDB */
}

/*
 * STUB: Read specific MAC from FDB
 */
void macfdb_read_specific_mac(struct zebra_ns *zns,
                             struct interface *br_if,
                             const struct ethaddr *mac, vlanid_t vid)
{
	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: STUB - macfdb_read_specific_mac(" MACSTR
		           ", vid=%u)",
		           MAC2STR(mac->octet), vid);

	/* Real implementation would query OVSDB for specific MAC */
}

/*
 * STUB: Read neighbor table (ARP/ND)
 */
void neigh_read(struct zebra_ns *zns)
{
	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: STUB - neigh_read() for ns_id %u",
		           zns->ns_id);

	/* Real implementation would read from OpenFlow flows or OVSDB */
}

/*
 * STUB: Read neighbors for specific VLAN
 */
void neigh_read_for_vlan(struct zebra_ns *zns, struct interface *ifp)
{
	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: STUB - neigh_read_for_vlan(%s)",
		           ifp ? ifp->name : "NULL");

	/* Real implementation would filter by VLAN interface */
}

/*
 * STUB: Read specific neighbor entry
 */
void neigh_read_specific_ip(const struct ipaddr *ip,
                           struct interface *vlan_if)
{
	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: STUB - neigh_read_specific_ip(%pIA, %s)",
		           ip, vlan_if ? vlan_if->name : "NULL");

	/* Real implementation would query for specific IP */
}

/*
 * STUB: Read routing table (not needed for EVPN)
 */
void route_read(struct zebra_ns *zns)
{
	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: STUB - route_read() for ns_id %u",
		           zns->ns_id);

	/* OVS doesn't have routing table - this is a no-op */
}

/*
 * STUB: Read VLAN table
 */
void vlan_read(struct zebra_ns *zns)
{
	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: STUB - vlan_read() for ns_id %u",
		           zns->ns_id);

	/* Real implementation would read VLAN info from OVSDB */
}

/*
 * STUB: Dataplane read (called by dplane thread)
 */
int kernel_dplane_read(struct zebra_dplane_info *info)
{
	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: STUB - kernel_dplane_read()");

	/*
	 * Real implementation would:
	 * - Process pending OVSDB notifications
	 * - Process OpenFlow messages
	 */

	return 0;
}

#endif /* HAVE_OVS */
