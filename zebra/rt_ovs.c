// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Zebra OVS backend - Route and Nexthop operations
 * Copyright (C) 2025 FRRouting
 */

#include <zebra.h>

#ifdef HAVE_OVS

#include "log.h"
#include "rib.h"
#include "rt.h"

#include "zebra/zebra_ns.h"
#include "zebra/zebra_vrf.h"
#include "zebra/zebra_errors.h"
#include "zebra/kernel_ovs.h"
#include "zebra/zebra_dplane.h"

/*
 * STUB: FDB Nexthop creation (Phase 2 - not implemented)
 *
 * In full implementation, this would create an OpenFlow group (type=select)
 * with a single bucket pointing to the VTEP IP.
 */
int kernel_upd_mac_nh(uint32_t nh_id, struct ipaddr *vtep_ip)
{
	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: STUB - kernel_upd_mac_nh(nh_id=%u, vtep=%pIA)",
		           nh_id, vtep_ip);

	/*
	 * Real implementation would:
	 *
	 * 1. Get OVS context:
	 *    struct zebra_ns *zns = zebra_ns_lookup(NS_DEFAULT);
	 *    struct ovs_ctx *ctx = zns->ovs_ctx;
	 *
	 * 2. Create OpenFlow group message:
	 *    struct ofputil_group_mod gm;
	 *    memset(&gm, 0, sizeof(gm));
	 *    gm.command = OFPGC15_ADD_OR_MOD;
	 *    gm.type = OFPGT11_SELECT;
	 *    gm.group_id = nh_id;
	 *
	 * 3. Add bucket with action: set_field(tun_dst=vtep_ip), output(vxlan_port)
	 *
	 * 4. Send to OVS via OpenFlow
	 */

	/* Stub: Always return success */
	return 0;
}

/*
 * STUB: FDB Nexthop deletion (Phase 2 - not implemented)
 */
int kernel_del_mac_nh(uint32_t nh_id)
{
	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: STUB - kernel_del_mac_nh(nh_id=%u)", nh_id);

	/*
	 * Real implementation would delete OpenFlow group with group_id=nh_id
	 */

	/* Stub: Always return success */
	return 0;
}

/*
 * STUB: FDB Nexthop Group creation (Phase 2 - not implemented)
 *
 * In full implementation, this would create an OpenFlow group (type=fast_failover)
 * with multiple buckets, one per member NH.
 */
int kernel_upd_mac_nhg(uint32_t nhg_id, uint32_t nh_cnt, struct nh_grp *nh_ids)
{
	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: STUB - kernel_upd_mac_nhg(nhg_id=%u, nh_cnt=%u)",
		           nhg_id, nh_cnt);

	/*
	 * Real implementation would:
	 *
	 * 1. Create OpenFlow group (type=fast_failover)
	 * 2. Add bucket for each member NH:
	 *    for (i = 0; i < nh_cnt; i++) {
	 *        // Bucket action: group=nh_ids[i].id
	 *        // Watch: watch_group=nh_ids[i].id
	 *    }
	 * 3. Send to OVS
	 */

	/* Stub: Always return success */
	return 0;
}

/*
 * STUB: FDB Nexthop Group deletion (Phase 2 - not implemented)
 */
int kernel_del_mac_nhg(uint32_t nhg_id)
{
	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: STUB - kernel_del_mac_nhg(nhg_id=%u)", nhg_id);

	/* Stub: Always return success */
	return 0;
}

/*
 * STUB: Route update (not needed for EVPN, but required by interface)
 */
#ifndef HAVE_NETLINK
enum zebra_dplane_result kernel_route_update(struct zebra_dplane_ctx *ctx)
{
	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: STUB - kernel_route_update()");

	/* OVS doesn't handle IP routes - return success to avoid errors */
	return ZEBRA_DPLANE_REQUEST_SUCCESS;
}

/*
 * STUB: Nexthop update (not needed for EVPN, but required by interface)
 */
enum zebra_dplane_result kernel_nexthop_update(struct zebra_dplane_ctx *ctx)
{
	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: STUB - kernel_nexthop_update()");

	/* Return success */
	return ZEBRA_DPLANE_REQUEST_SUCCESS;
}

/*
 * STUB: LSP update (MPLS - not needed for EVPN)
 */
enum zebra_dplane_result kernel_lsp_update(struct zebra_dplane_ctx *ctx)
{
	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: STUB - kernel_lsp_update()");

	return ZEBRA_DPLANE_REQUEST_SUCCESS;
}

/*
 * STUB: Pseudowire update (not needed for EVPN)
 */
enum zebra_dplane_result kernel_pw_update(struct zebra_dplane_ctx *ctx)
{
	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: STUB - kernel_pw_update()");

	return ZEBRA_DPLANE_REQUEST_SUCCESS;
}

/*
 * STUB: Address update (not needed for EVPN)
 */
enum zebra_dplane_result kernel_address_update_ctx(struct zebra_dplane_ctx *ctx)
{
	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: STUB - kernel_address_update_ctx()");

	return ZEBRA_DPLANE_REQUEST_SUCCESS;
}

/*
 * STUB: PBR rule update (not needed for EVPN)
 */
enum zebra_dplane_result kernel_pbr_rule_update(struct zebra_dplane_ctx *ctx)
{
	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: STUB - kernel_pbr_rule_update()");

	return ZEBRA_DPLANE_REQUEST_SUCCESS;
}

/*
 * STUB: Interface netconf update (not needed for EVPN)
 */
enum zebra_dplane_result kernel_intf_netconf_update(struct zebra_dplane_ctx *ctx)
{
	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: STUB - kernel_intf_netconf_update()");

	return ZEBRA_DPLANE_REQUEST_SUCCESS;
}

/*
 * STUB: Traffic control update (not needed for EVPN)
 */
enum zebra_dplane_result kernel_tc_update(struct zebra_dplane_ctx *ctx)
{
	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: STUB - kernel_tc_update()");

	return ZEBRA_DPLANE_REQUEST_SUCCESS;
}

#endif /* !HAVE_NETLINK */

#endif /* HAVE_OVS */
