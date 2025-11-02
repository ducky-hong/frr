// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Zebra OVS backend implementation
 * Copyright (C) 2025 FRRouting
 */

#include <zebra.h>

#ifdef HAVE_OVS

#include "log.h"
#include "memory.h"
#include "frrevent.h"
#include "prefix.h"
#include "vrf.h"

#include "zebra/zebra_ns.h"
#include "zebra/zebra_vrf.h"
#include "zebra/zebra_errors.h"
#include "zebra/kernel_ovs.h"
#include "zebra/zebra_dplane.h"

/*
 * NOTE: The OVS library includes would go here in a real implementation.
 * For this stub, we'll use placeholder types.
 *
 * Real includes would be:
 * #include <openvswitch/ovsdb-idl.h>
 * #include <openvswitch/vconn.h>
 * #include <openvswitch/ofp-msgs.h>
 * #include <ovsdb/ovsdb.h>
 * #include "lib/ovsdb-idl.h"
 * #include "openvswitch/vlog.h"
 */

DEFINE_MTYPE_STATIC(ZEBRA, OVS_CTX, "OVS Context");

/* OVS connection state */
struct ovs_ctx {
	/* OVSDB connection */
	void *ovs_idl;  /* struct ovsdb_idl * in real code */
	char ovsdb_socket[256];
	char bridge_name[64];

	/* OpenFlow connection */
	void *of_conn;  /* struct vconn * in real code */
	char of_socket[256];

	/* Event handlers */
	struct event *t_ovs_idl;

	/* Cached info */
	uint64_t datapath_id;
	uint16_t vxlan_ofport;

	/* Stats */
	uint64_t idl_seqno;
	uint64_t changes_processed;
};

/*
 * Initialize OVS backend for namespace
 */
void ovs_kernel_init(struct zebra_ns *zns)
{
	struct ovs_ctx *ctx;

	zlog_info("OVS: Initializing OVS backend for ns_id %u", zns->ns_id);

	/* Allocate OVS context */
	ctx = XCALLOC(MTYPE_OVS_CTX, sizeof(struct ovs_ctx));
	zns->ovs_ctx = ctx;

	/* Set default bridge name (TODO: make configurable) */
	snprintf(ctx->bridge_name, sizeof(ctx->bridge_name),
	         "%s", OVS_DEFAULT_BRIDGE);

	/* Set OVSDB socket path */
	snprintf(ctx->ovsdb_socket, sizeof(ctx->ovsdb_socket),
	         "unix:%s", OVS_OVSDB_SOCKET);

	/*
	 * Real OVSDB connection would be:
	 *
	 * ctx->ovs_idl = ovsdb_idl_create(ctx->ovsdb_socket,
	 *                                 &ovsrec_idl_class,
	 *                                 false,  // not remote
	 *                                 true);  // enable monitoring
	 *
	 * if (!ctx->ovs_idl) {
	 *     flog_err(EC_ZEBRA_OVSDB_CONNECT_FAIL,
	 *              "Failed to connect to OVSDB at %s",
	 *              ctx->ovsdb_socket);
	 *     XFREE(MTYPE_OVS_CTX, ctx);
	 *     zns->ovs_ctx = NULL;
	 *     return;
	 * }
	 *
	 * // Track relevant OVSDB tables
	 * ovsdb_idl_add_table(ctx->ovs_idl, &ovsrec_table_bridge);
	 * ovsdb_idl_add_table(ctx->ovs_idl, &ovsrec_table_port);
	 * ovsdb_idl_add_table(ctx->ovs_idl, &ovsrec_table_interface);
	 * ovsdb_idl_add_table(ctx->ovs_idl, &ovsrec_table_logical_switch);
	 * ovsdb_idl_add_table(ctx->ovs_idl, &ovsrec_table_ucast_macs_local);
	 * ovsdb_idl_add_table(ctx->ovs_idl, &ovsrec_table_ucast_macs_remote);
	 * ovsdb_idl_add_table(ctx->ovs_idl, &ovsrec_table_physical_locator);
	 */

	/* Set OpenFlow management socket */
	snprintf(ctx->of_socket, sizeof(ctx->of_socket),
	         "unix:/var/run/openvswitch/%s.mgmt",
	         ctx->bridge_name);

	/*
	 * Real OpenFlow connection would be:
	 *
	 * int error = vconn_open(ctx->of_socket,
	 *                       OFP15_VERSION,
	 *                       DSCP_DEFAULT,
	 *                       &ctx->of_conn);
	 * if (error) {
	 *     flog_err(EC_ZEBRA_OF_CONNECT_FAIL,
	 *              "Failed to connect to OpenFlow: %s",
	 *              ovs_strerror(error));
	 *     // Non-fatal - can still use OVSDB
	 * }
	 */

	/* Register OVSDB polling timer */
	event_add_timer_msec(zrouter.master, ovs_idl_run, zns,
	                     OVS_IDL_POLL_INTERVAL_MS,
	                     &ctx->t_ovs_idl);

	zlog_info("OVS: Backend initialized for bridge %s", ctx->bridge_name);
}

/*
 * Terminate OVS backend
 */
void ovs_kernel_terminate(struct zebra_ns *zns)
{
	struct ovs_ctx *ctx = zns->ovs_ctx;

	if (!ctx)
		return;

	zlog_info("OVS: Terminating OVS backend for ns_id %u", zns->ns_id);

	/* Cancel event timers */
	EVENT_OFF(ctx->t_ovs_idl);

	/*
	 * Real cleanup would be:
	 *
	 * // Close OpenFlow connection
	 * if (ctx->of_conn) {
	 *     vconn_close(ctx->of_conn);
	 *     ctx->of_conn = NULL;
	 * }
	 *
	 * // Close OVSDB connection
	 * if (ctx->ovs_idl) {
	 *     ovsdb_idl_destroy(ctx->ovs_idl);
	 *     ctx->ovs_idl = NULL;
	 * }
	 */

	/* Free context */
	XFREE(MTYPE_OVS_CTX, ctx);
	zns->ovs_ctx = NULL;

	zlog_info("OVS: Backend terminated");
}

/*
 * OVSDB event loop callback
 */
void ovs_idl_run(struct event *thread)
{
	struct zebra_ns *zns = EVENT_ARG(thread);
	struct ovs_ctx *ctx = zns->ovs_ctx;

	if (!ctx)
		return;

	/*
	 * Real OVSDB polling would be:
	 *
	 * // Run OVSDB state machine
	 * ovsdb_idl_run(ctx->ovs_idl);
	 *
	 * // Check for changes
	 * unsigned int seqno = ovsdb_idl_get_seqno(ctx->ovs_idl);
	 * if (seqno != ctx->idl_seqno) {
	 *     // Database changed - process updates
	 *     ovs_process_changes(zns);
	 *     ctx->idl_seqno = seqno;
	 *     ctx->changes_processed++;
	 * }
	 *
	 * // Process any pending transactions
	 * ovsdb_idl_wait(ctx->ovs_idl);
	 */

	/* Re-register timer */
	event_add_timer_msec(zrouter.master, ovs_idl_run, zns,
	                     OVS_IDL_POLL_INTERVAL_MS,
	                     &ctx->t_ovs_idl);
}

/*
 * Process OVSDB changes
 */
void ovs_process_changes(struct zebra_ns *zns)
{
	struct ovs_ctx *ctx = zns->ovs_ctx;

	if (!ctx)
		return;

	/*
	 * Real change processing would iterate through OVSDB tables:
	 *
	 * const struct ovsrec_interface *iface_row;
	 * OVSREC_INTERFACE_FOR_EACH(iface_row, ctx->ovs_idl) {
	 *     if (ovsrec_interface_is_new(iface_row)) {
	 *         // New interface added
	 *         ovs_interface_add(zns, iface_row);
	 *     } else if (ovsrec_interface_is_deleted(iface_row)) {
	 *         // Interface deleted
	 *         ovs_interface_del(zns, iface_row);
	 *     } else if (ovsrec_interface_is_updated(iface_row,
	 *                OVSREC_INTERFACE_COL_OFPORT)) {
	 *         // Interface ofport changed
	 *         ovs_interface_update(zns, iface_row);
	 *     }
	 * }
	 *
	 * // Similar for MAC tables, etc.
	 */

	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("OVS: Processed OVSDB changes (seq=%lu)",
		           (unsigned long)ctx->changes_processed);
}

/*
 * Helper: Get OpenFlow port number from interface index
 */
uint16_t ovs_get_ofport_by_ifindex(struct zebra_ns *zns, ifindex_t ifindex)
{
	struct ovs_ctx *ctx = zns->ovs_ctx;

	if (!ctx)
		return 0;

	/*
	 * Real implementation would:
	 *
	 * const struct ovsrec_interface *iface_row;
	 * OVSREC_INTERFACE_FOR_EACH(iface_row, ctx->ovs_idl) {
	 *     // Match by external_ids:ifindex
	 *     const char *ifindex_str = smap_get(&iface_row->external_ids,
	 *                                        "ifindex");
	 *     if (ifindex_str && atoi(ifindex_str) == (int)ifindex) {
	 *         if (iface_row->n_ofport > 0)
	 *             return iface_row->ofport[0];
	 *     }
	 * }
	 */

	return 0;  // Not found
}

/*
 * Helper: Get interface name from OpenFlow port
 */
const char *ovs_get_ifname_by_ofport(struct zebra_ns *zns, uint16_t ofport)
{
	struct ovs_ctx *ctx = zns->ovs_ctx;

	if (!ctx)
		return NULL;

	/*
	 * Real implementation would:
	 *
	 * const struct ovsrec_interface *iface_row;
	 * OVSREC_INTERFACE_FOR_EACH(iface_row, ctx->ovs_idl) {
	 *     if (iface_row->n_ofport > 0 &&
	 *         iface_row->ofport[0] == ofport) {
	 *         return iface_row->name;
	 *     }
	 * }
	 */

	return NULL;  // Not found
}

/*
 * Helper: Send OpenFlow message
 */
int ovs_send_openflow_msg(struct zebra_ns *zns, struct ofpbuf *msg)
{
	struct ovs_ctx *ctx = zns->ovs_ctx;

	if (!ctx || !ctx->of_conn) {
		zlog_err("OVS: Cannot send OF message - not connected");
		return -1;
	}

	/*
	 * Real implementation would:
	 *
	 * int error = vconn_send_block(ctx->of_conn, msg);
	 * if (error) {
	 *     flog_err(EC_ZEBRA_OF_SEND_FAIL,
	 *              "Failed to send OF message: %s",
	 *              ovs_strerror(error));
	 *     return -1;
	 * }
	 */

	return 0;
}

/*
 * Helper: Parse MAC address from OVSDB string
 */
bool ovs_parse_mac(const char *mac_str, struct ethaddr *mac)
{
	if (!mac_str || !mac)
		return false;

	/*
	 * Parse MAC string like "00:11:22:33:44:55"
	 */
	unsigned int octets[ETH_ALEN];
	int ret = sscanf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x",
	                &octets[0], &octets[1], &octets[2],
	                &octets[3], &octets[4], &octets[5]);

	if (ret != ETH_ALEN)
		return false;

	for (int i = 0; i < ETH_ALEN; i++)
		mac->octet[i] = octets[i];

	return true;
}

/*
 * Helper: Parse IP address from OVSDB string
 */
bool ovs_parse_ip(const char *ip_str, struct ipaddr *ip)
{
	if (!ip_str || !ip)
		return false;

	/* Try IPv4 first */
	if (inet_pton(AF_INET, ip_str, &ip->ipaddr_v4) == 1) {
		ip->ipa_type = IPADDR_V4;
		return true;
	}

	/* Try IPv6 */
	if (inet_pton(AF_INET6, ip_str, &ip->ipaddr_v6) == 1) {
		ip->ipa_type = IPADDR_V6;
		return true;
	}

	return false;
}

/*
 * Find logical switch by VNI
 */
const struct ovsrec_logical_switch *
ovs_find_logical_switch_by_vni(struct zebra_ns *zns, vni_t vni)
{
	struct ovs_ctx *ctx = zns->ovs_ctx;

	if (!ctx)
		return NULL;

	/*
	 * Real implementation would:
	 *
	 * const struct ovsrec_logical_switch *ls;
	 * OVSREC_LOGICAL_SWITCH_FOR_EACH(ls, ctx->ovs_idl) {
	 *     // Check tunnel_key (VNI)
	 *     if (ls->n_tunnel_key > 0 &&
	 *         ls->tunnel_key[0] == vni) {
	 *         return ls;
	 *     }
	 * }
	 */

	return NULL;  // Not found
}

/*
 * Create logical switch for VNI
 */
const struct ovsrec_logical_switch *
ovs_create_logical_switch(struct ovsdb_idl_txn *txn, vni_t vni)
{
	/*
	 * Real implementation would:
	 *
	 * const struct ovsrec_logical_switch *ls;
	 * ls = ovsrec_logical_switch_insert(txn);
	 *
	 * // Set name
	 * char name[64];
	 * snprintf(name, sizeof(name), "vni-%u", vni);
	 * ovsrec_logical_switch_set_name(ls, name);
	 *
	 * // Set tunnel key (VNI)
	 * int64_t key = vni;
	 * ovsrec_logical_switch_set_tunnel_key(ls, &key, 1);
	 *
	 * return ls;
	 */

	return NULL;  // Stub
}

/*
 * Find physical locator by VTEP IP
 */
const struct ovsrec_physical_locator *
ovs_find_physical_locator_by_ip(struct zebra_ns *zns,
                                const struct ipaddr *vtep_ip)
{
	struct ovs_ctx *ctx = zns->ovs_ctx;

	if (!ctx)
		return NULL;

	/*
	 * Real implementation would:
	 *
	 * char ip_str[INET6_ADDRSTRLEN];
	 * ipaddr2str(vtep_ip, ip_str, sizeof(ip_str));
	 *
	 * const struct ovsrec_physical_locator *pl;
	 * OVSREC_PHYSICAL_LOCATOR_FOR_EACH(pl, ctx->ovs_idl) {
	 *     if (strcmp(pl->dst_ip, ip_str) == 0)
	 *         return pl;
	 * }
	 */

	return NULL;  // Not found
}

/*
 * Create physical locator for VTEP IP
 */
const struct ovsrec_physical_locator *
ovs_create_physical_locator(struct ovsdb_idl_txn *txn,
                            const struct ipaddr *vtep_ip)
{
	/*
	 * Real implementation would:
	 *
	 * const struct ovsrec_physical_locator *pl;
	 * pl = ovsrec_physical_locator_insert(txn);
	 *
	 * // Set destination IP
	 * char ip_str[INET6_ADDRSTRLEN];
	 * ipaddr2str(vtep_ip, ip_str, sizeof(ip_str));
	 * ovsrec_physical_locator_set_dst_ip(pl, ip_str);
	 *
	 * // Set encap type (vxlan_over_ipv4 or vxlan_over_ipv6)
	 * if (vtep_ip->ipa_type == IPADDR_V4)
	 *     ovsrec_physical_locator_set_encapsulation_type(pl,
	 *                                                    "vxlan_over_ipv4");
	 * else
	 *     ovsrec_physical_locator_set_encapsulation_type(pl,
	 *                                                    "vxlan_over_ipv6");
	 *
	 * return pl;
	 */

	return NULL;  // Stub
}

#endif /* HAVE_OVS */
