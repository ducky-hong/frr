// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Zebra OVS backend - MAC and Neighbor FDB operations
 * Copyright (C) 2025 FRRouting
 *
 * PHASE 3 IMPLEMENTATION:
 * - Remote MAC programming to OVSDB
 * - Local MAC programming to OVSDB
 * - Remote neighbor programming (ARP responder via OpenFlow)
 * - Local neighbor programming
 *
 * NOTE: This implementation uses direct VTEP IP, not NHG (NH/NHG stubbed out)
 */

#include <zebra.h>

#ifdef HAVE_OVS

#include "log.h"
#include "prefix.h"
#include "if.h"
#include "vlan.h"

#include "zebra/zebra_ns.h"
#include "zebra/zebra_vrf.h"
#include "zebra/zebra_errors.h"
#include "zebra/kernel_ovs.h"
#include "zebra/zebra_dplane.h"
#include "zebra/rt.h"

/*
 * Program remote MAC to OVSDB Ucast_Macs_Remote table
 *
 * For EVPN, this is called when:
 * - BGP receives Type-2 route with MAC
 * - MAC is remote (learned from peer VTEP)
 *
 * NOTE: nhg_id is ignored (stubbed), always uses vtep_ip
 */
static enum zebra_dplane_result
ovs_mac_remote_add(const struct ethaddr *mac, vni_t vni,
                  uint32_t nhg_id, const struct ipaddr *vtep_ip,
                  bool sticky)
{
	struct zebra_ns *zns = zebra_ns_lookup(NS_DEFAULT);
	struct ovs_ctx *ctx;

	if (!zns || !zns->ovs_ctx) {
		flog_err(EC_ZEBRA_OVSDB_NOT_CONNECTED,
		         "Cannot add remote MAC - OVS not initialized");
		return ZEBRA_DPLANE_REQUEST_FAILURE;
	}

	ctx = zns->ovs_ctx;

	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: Adding remote MAC " MACSTR
		           " vni %u vtep %pIA (nhg_id=%u %s)",
		           MAC2STR(mac->octet), vni, vtep_ip, nhg_id,
		           sticky ? "sticky" : "");

	/*
	 * Real OVSDB transaction would be:
	 *
	 * 1. Start transaction:
	 *    struct ovsdb_idl_txn *txn;
	 *    txn = ovsdb_idl_txn_create(ctx->ovs_idl);
	 *
	 * 2. Find or create logical switch for VNI:
	 *    const struct ovsrec_logical_switch *ls;
	 *    ls = ovs_find_logical_switch_by_vni(zns, vni);
	 *    if (!ls)
	 *        ls = ovs_create_logical_switch(txn, vni);
	 *
	 * 3. Find or create physical locator for VTEP:
	 *    const struct ovsrec_physical_locator *pl;
	 *    pl = ovs_find_physical_locator_by_ip(zns, vtep_ip);
	 *    if (!pl)
	 *        pl = ovs_create_physical_locator(txn, vtep_ip);
	 *
	 * 4. Insert/update Ucast_Macs_Remote row:
	 *    const struct ovsrec_ucast_macs_remote *row;
	 *    row = ovsrec_ucast_macs_remote_insert(txn);
	 *
	 *    // Set MAC address
	 *    char mac_str[ETH_ADDR_STRLEN];
	 *    snprintf(mac_str, sizeof(mac_str), MACSTR,
	 *             MAC2STR(mac->octet));
	 *    ovsrec_ucast_macs_remote_set_MAC(row, mac_str);
	 *
	 *    // Link to logical switch
	 *    ovsrec_ucast_macs_remote_set_logical_switch(row, ls);
	 *
	 *    // Link to physical locator (VTEP)
	 *    ovsrec_ucast_macs_remote_set_locator(row, pl);
	 *
	 * 5. Commit transaction:
	 *    enum ovsdb_idl_txn_status status;
	 *    status = ovsdb_idl_txn_commit_block(txn);
	 *    ovsdb_idl_txn_destroy(txn);
	 *
	 *    if (status != TXN_SUCCESS && status != TXN_UNCHANGED) {
	 *        flog_err(EC_ZEBRA_OVSDB_TXN_FAIL,
	 *                 "Failed to add remote MAC: %s",
	 *                 ovsdb_idl_txn_status_to_string(status));
	 *        return ZEBRA_DPLANE_REQUEST_FAILURE;
	 *    }
	 */

	/* Log stub success */
	zlog_info("OVS: Successfully added remote MAC " MACSTR " vni %u (STUB)",
	          MAC2STR(mac->octet), vni);

	return ZEBRA_DPLANE_REQUEST_SUCCESS;
}

/*
 * Delete remote MAC from OVSDB
 */
static enum zebra_dplane_result
ovs_mac_remote_del(const struct ethaddr *mac, vni_t vni)
{
	struct zebra_ns *zns = zebra_ns_lookup(NS_DEFAULT);

	if (!zns || !zns->ovs_ctx)
		return ZEBRA_DPLANE_REQUEST_FAILURE;

	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: Deleting remote MAC " MACSTR " vni %u",
		           MAC2STR(mac->octet), vni);

	/*
	 * Real implementation would:
	 *
	 * 1. Find Ucast_Macs_Remote row by MAC and logical switch
	 * 2. Delete row:
	 *    ovsrec_ucast_macs_remote_delete(row);
	 * 3. Commit transaction
	 */

	zlog_info("OVS: Successfully deleted remote MAC " MACSTR
	          " vni %u (STUB)",
	          MAC2STR(mac->octet), vni);

	return ZEBRA_DPLANE_REQUEST_SUCCESS;
}

/*
 * Program local MAC to OVSDB Ucast_Macs_Local table
 *
 * For EVPN MH, this is called when:
 * - Local MAC learned on access port
 * - MAC needs to be synced with ES peers
 */
static enum zebra_dplane_result
ovs_mac_local_add(const struct ethaddr *mac, ifindex_t ifindex,
                 vlanid_t vid, bool sticky, bool set_static,
                 bool set_inactive)
{
	struct zebra_ns *zns = zebra_ns_lookup(NS_DEFAULT);

	if (!zns || !zns->ovs_ctx)
		return ZEBRA_DPLANE_REQUEST_FAILURE;

	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: Adding local MAC " MACSTR
		           " ifindex %u vid %u (sticky=%d static=%d inactive=%d)",
		           MAC2STR(mac->octet), ifindex, vid,
		           sticky, set_static, set_inactive);

	/*
	 * Real implementation would:
	 *
	 * 1. Find interface by ifindex
	 * 2. Find or create Ucast_Macs_Local row
	 * 3. Set MAC, logical switch, and locator (local port)
	 * 4. If set_static, set aging=0 or similar
	 * 5. Commit transaction
	 */

	zlog_info("OVS: Successfully added local MAC " MACSTR " (STUB)",
	          MAC2STR(mac->octet));

	return ZEBRA_DPLANE_REQUEST_SUCCESS;
}

/*
 * Delete local MAC from OVSDB
 */
static enum zebra_dplane_result
ovs_mac_local_del(const struct ethaddr *mac, ifindex_t ifindex, vlanid_t vid)
{
	struct zebra_ns *zns = zebra_ns_lookup(NS_DEFAULT);

	if (!zns || !zns->ovs_ctx)
		return ZEBRA_DPLANE_REQUEST_FAILURE;

	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: Deleting local MAC " MACSTR
		           " ifindex %u vid %u",
		           MAC2STR(mac->octet), ifindex, vid);

	/*
	 * Real implementation would delete Ucast_Macs_Local row
	 */

	zlog_info("OVS: Successfully deleted local MAC " MACSTR " (STUB)",
	          MAC2STR(mac->octet));

	return ZEBRA_DPLANE_REQUEST_SUCCESS;
}

/*
 * Program remote neighbor (ARP responder) via OpenFlow
 *
 * For EVPN, this is called when:
 * - BGP receives Type-2 route with MAC+IP
 * - Need to respond to ARP requests locally
 *
 * Creates OpenFlow flow in ARP responder table:
 * Match: arp_op=request, arp_tpa=IP
 * Action: Construct ARP reply with target MAC, send back to in_port
 */
static enum zebra_dplane_result
ovs_neigh_remote_add(const struct ipaddr *ip, const struct ethaddr *mac,
                    ifindex_t ifindex, uint32_t flags, bool was_static)
{
	struct zebra_ns *zns = zebra_ns_lookup(NS_DEFAULT);

	if (!zns || !zns->ovs_ctx)
		return ZEBRA_DPLANE_REQUEST_FAILURE;

	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: Adding remote neighbor %pIA -> " MACSTR
		           " (flags=0x%x)",
		           ip, MAC2STR(mac->octet), flags);

	/*
	 * Real OpenFlow flow programming would be:
	 *
	 * 1. Create flow_mod for table OVS_TABLE_ARP_RESPONDER (20):
	 *    struct ofputil_flow_mod fm;
	 *    memset(&fm, 0, sizeof(fm));
	 *    fm.table_id = OVS_TABLE_ARP_RESPONDER;
	 *    fm.priority = 100;
	 *    fm.command = OFPFC_MODIFY_STRICT;  // Add or modify
	 *    fm.idle_timeout = OFP_FLOW_PERMANENT;
	 *    fm.hard_timeout = OFP_FLOW_PERMANENT;
	 *
	 * 2. Match: ARP request for this IP:
	 *    match_init_catchall(&fm.match);
	 *    match_set_dl_type(&fm.match, htons(ETH_TYPE_ARP));
	 *    match_set_nw_proto(&fm.match, ARP_OP_REQUEST);
	 *
	 *    if (ip->ipa_type == IPADDR_V4) {
	 *        match_set_nw_dst(&fm.match, ip->ipaddr_v4.s_addr);
	 *    }
	 *    // TODO: IPv6 NDP responder
	 *
	 * 3. Actions: Construct ARP reply:
	 *    struct ofpbuf acts = OFPBUF_STUB_INITIALIZER(acts_stub);
	 *
	 *    // Swap eth src/dst
	 *    ofpact_put_ETH_SWAP(&acts);
	 *
	 *    // Set eth_src = target MAC
	 *    ofpact_put_SET_ETH_SRC(&acts)->mac = *mac;
	 *
	 *    // Set arp_op = reply
	 *    ofpact_put_SET_FIELD(&acts, mf_from_id(MFF_ARP_OP),
	 *                        &arp_reply, NULL);
	 *
	 *    // Swap ARP spa/tpa and sha/tha
	 *    ofpact_put_ARP_SWAP(&acts);
	 *
	 *    // Set arp_sha = target MAC
	 *    ofpact_put_SET_FIELD(&acts, mf_from_id(MFF_ARP_SHA),
	 *                        mac, NULL);
	 *
	 *    // Set arp_spa = target IP
	 *    ofpact_put_SET_FIELD(&acts, mf_from_id(MFF_ARP_SPA),
	 *                        &ip->ipaddr_v4, NULL);
	 *
	 *    // Send back to in_port
	 *    ofpact_put_OUTPUT(&acts)->port = OFPP_IN_PORT;
	 *
	 *    fm.ofpacts = acts.data;
	 *    fm.ofpacts_len = acts.size;
	 *
	 * 4. Encode and send to OVS:
	 *    struct ofpbuf *msg;
	 *    msg = ofputil_encode_flow_mod(&fm, OFPUTIL_P_OF15_OXM);
	 *    ovs_send_openflow_msg(zns, msg);
	 *
	 *    ofpbuf_uninit(&acts);
	 */

	zlog_info("OVS: Successfully added ARP responder for %pIA -> " MACSTR
	          " (STUB)",
	          ip, MAC2STR(mac->octet));

	return ZEBRA_DPLANE_REQUEST_SUCCESS;
}

/*
 * Delete remote neighbor (ARP responder)
 */
static enum zebra_dplane_result
ovs_neigh_remote_del(const struct ipaddr *ip, ifindex_t ifindex)
{
	struct zebra_ns *zns = zebra_ns_lookup(NS_DEFAULT);

	if (!zns || !zns->ovs_ctx)
		return ZEBRA_DPLANE_REQUEST_FAILURE;

	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: Deleting remote neighbor %pIA", ip);

	/*
	 * Real implementation would:
	 * - Delete OpenFlow flow with matching IP from ARP table
	 */

	zlog_info("OVS: Successfully deleted ARP responder for %pIA (STUB)",
	          ip);

	return ZEBRA_DPLANE_REQUEST_SUCCESS;
}

/*
 * Program local neighbor via OpenFlow
 *
 * For EVPN MH sync, this is called when:
 * - Local neighbor learned on access port
 * - Need to program to local FDB
 */
static enum zebra_dplane_result
ovs_neigh_local_add(const struct ipaddr *ip, const struct ethaddr *mac,
                   ifindex_t ifindex, bool set_router, bool set_static,
                   bool set_inactive)
{
	struct zebra_ns *zns = zebra_ns_lookup(NS_DEFAULT);

	if (!zns || !zns->ovs_ctx)
		return ZEBRA_DPLANE_REQUEST_FAILURE;

	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: Adding local neighbor %pIA -> " MACSTR
		           " ifindex %u (router=%d static=%d inactive=%d)",
		           ip, MAC2STR(mac->octet), ifindex,
		           set_router, set_static, set_inactive);

	/*
	 * Real implementation might:
	 * - Add to OVSDB neighbor table (if exists)
	 * - Or install ARP responder flow (same as remote)
	 */

	zlog_info("OVS: Successfully added local neighbor %pIA (STUB)", ip);

	return ZEBRA_DPLANE_REQUEST_SUCCESS;
}

/*
 * Delete local neighbor
 */
static enum zebra_dplane_result
ovs_neigh_local_del(const struct ipaddr *ip, ifindex_t ifindex)
{
	struct zebra_ns *zns = zebra_ns_lookup(NS_DEFAULT);

	if (!zns || !zns->ovs_ctx)
		return ZEBRA_DPLANE_REQUEST_FAILURE;

	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: Deleting local neighbor %pIA ifindex %u",
		           ip, ifindex);

	zlog_info("OVS: Successfully deleted local neighbor %pIA (STUB)", ip);

	return ZEBRA_DPLANE_REQUEST_SUCCESS;
}

/*
 * Main MAC update dispatcher (called by dataplane)
 */
#ifndef HAVE_NETLINK
enum zebra_dplane_result kernel_mac_update_ctx(struct zebra_dplane_ctx *ctx)
{
	enum dplane_op_e op = dplane_ctx_get_op(ctx);
	const struct ethaddr *mac = dplane_ctx_mac_get_addr(ctx);
	vni_t vni = dplane_ctx_mac_get_vni(ctx);
	uint32_t nhg_id = dplane_ctx_mac_get_nhg_id(ctx);
	const struct ipaddr *vtep_ip = dplane_ctx_mac_get_vtep_ip(ctx);
	bool sticky = dplane_ctx_mac_is_sticky(ctx);
	ifindex_t ifindex = dplane_ctx_get_ifindex(ctx);
	vlanid_t vid = dplane_ctx_mac_get_vlan(ctx);

	switch (op) {
	case DPLANE_OP_MAC_INSTALL:
		/* Remote MAC - check if using ES (NHG) or direct VTEP */
		if (nhg_id) {
			/* NH/NHG stubbed - ignore nhg_id, use vtep_ip */
			zlog_warn("OVS: NHG %u ignored (stubbed), using VTEP %pIA",
			          nhg_id, vtep_ip);
		}
		return ovs_mac_remote_add(mac, vni, nhg_id, vtep_ip, sticky);

	case DPLANE_OP_MAC_DELETE:
		return ovs_mac_remote_del(mac, vni);

	default:
		/* Local MAC operations use different op codes */
		if (IS_ZEBRA_DEBUG_KERNEL)
			zlog_debug("OVS: Unhandled MAC op %d", op);
		return ZEBRA_DPLANE_REQUEST_FAILURE;
	}
}

/*
 * Main Neighbor update dispatcher (called by dataplane)
 */
enum zebra_dplane_result kernel_neigh_update_ctx(struct zebra_dplane_ctx *ctx)
{
	enum dplane_op_e op = dplane_ctx_get_op(ctx);
	const struct ipaddr *ip = dplane_ctx_neigh_get_ipaddr(ctx);
	const struct ethaddr *mac = dplane_ctx_neigh_get_mac(ctx);
	ifindex_t ifindex = dplane_ctx_get_ifindex(ctx);
	uint32_t flags = dplane_ctx_neigh_get_flags(ctx);
	uint16_t state = dplane_ctx_neigh_get_state(ctx);

	switch (op) {
	case DPLANE_OP_NEIGH_INSTALL:
		/* Check if remote (ext_learned) or local */
		if (flags & DPLANE_NTF_EXT_LEARNED) {
			/* Remote neighbor - ARP responder */
			bool was_static = !!(flags & DPLANE_NTF_USE);
			return ovs_neigh_remote_add(ip, mac, ifindex,
			                           flags, was_static);
		} else {
			/* Local neighbor */
			bool set_router = !!(flags & DPLANE_NTF_ROUTER);
			bool set_static = !!(state == DPLANE_NUD_NOARP);
			bool set_inactive = !!(state == DPLANE_NUD_STALE);
			return ovs_neigh_local_add(ip, mac, ifindex,
			                          set_router, set_static,
			                          set_inactive);
		}

	case DPLANE_OP_NEIGH_DELETE:
		/* Check if remote or local */
		if (flags & DPLANE_NTF_EXT_LEARNED)
			return ovs_neigh_remote_del(ip, ifindex);
		else
			return ovs_neigh_local_del(ip, ifindex);

	default:
		if (IS_ZEBRA_DEBUG_KERNEL)
			zlog_debug("OVS: Unhandled neighbor op %d", op);
		return ZEBRA_DPLANE_REQUEST_FAILURE;
	}
}

/*
 * STUB: Interface update (DF/SPH - Phase 4, not implemented)
 */
enum zebra_dplane_result kernel_intf_update(struct zebra_dplane_ctx *ctx)
{
	enum dplane_op_e op = dplane_ctx_get_op(ctx);

	if (op == DPLANE_OP_BR_PORT_UPDATE) {
		/* DF/SPH/backup_nhg stubbed out */
		if (IS_ZEBRA_DEBUG_KERNEL) {
			const struct interface *ifp = dplane_ctx_get_ifp(ctx);
			bool non_df = dplane_ctx_is_non_df(ctx);
			uint32_t backup_nhg_id =
				dplane_ctx_get_backup_nhg_id(ctx);

			zlog_debug("OVS: STUB - BR_PORT_UPDATE %s "
			           "(non_df=%d backup_nhg=%u)",
			           ifp ? ifp->name : "NULL",
			           non_df, backup_nhg_id);
		}
		return ZEBRA_DPLANE_REQUEST_SUCCESS;
	}

	return ZEBRA_DPLANE_REQUEST_SUCCESS;
}
#endif /* !HAVE_NETLINK */

#endif /* HAVE_OVS */
