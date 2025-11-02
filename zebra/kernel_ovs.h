// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Zebra OVS backend header
 * Copyright (C) 2025 FRRouting
 */

#ifndef _ZEBRA_KERNEL_OVS_H
#define _ZEBRA_KERNEL_OVS_H

#ifdef HAVE_OVS

#include "zebra.h"
#include "zebra/zebra_ns.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * OVS Backend Configuration
 */
#define OVS_DEFAULT_BRIDGE "br-int"
#define OVS_OVSDB_SOCKET "/var/run/openvswitch/db.sock"
#define OVS_IDL_POLL_INTERVAL_MS 100

/*
 * OpenFlow Table IDs (for flow programming)
 */
#define OVS_TABLE_INGRESS 0
#define OVS_TABLE_ARP_RESPONDER 20
#define OVS_TABLE_MAC_LEARNING 30
#define OVS_TABLE_L2_FWD 40

/*
 * OVS Backend Functions
 */

/* Initialize OVS backend for namespace */
void ovs_kernel_init(struct zebra_ns *zns);

/* Terminate OVS backend */
void ovs_kernel_terminate(struct zebra_ns *zns);

/* OVSDB event loop callback */
void ovs_idl_run(struct event *thread);

/* Process OVSDB changes */
void ovs_process_changes(struct zebra_ns *zns);

/* Helper: Get OpenFlow port number from interface index */
uint16_t ovs_get_ofport_by_ifindex(struct zebra_ns *zns, ifindex_t ifindex);

/* Helper: Get interface name from OpenFlow port */
const char *ovs_get_ifname_by_ofport(struct zebra_ns *zns, uint16_t ofport);

/* Helper: Send OpenFlow message */
int ovs_send_openflow_msg(struct zebra_ns *zns, struct ofpbuf *msg);

/* Helper: Parse MAC address from OVSDB string */
bool ovs_parse_mac(const char *mac_str, struct ethaddr *mac);

/* Helper: Parse IP address from OVSDB string */
bool ovs_parse_ip(const char *ip_str, struct ipaddr *ip);

/*
 * OVSDB Table Helpers
 */

/* Find logical switch by VNI */
const struct ovsrec_logical_switch *
ovs_find_logical_switch_by_vni(struct zebra_ns *zns, vni_t vni);

/* Create logical switch for VNI */
const struct ovsrec_logical_switch *
ovs_create_logical_switch(struct ovsdb_idl_txn *txn, vni_t vni);

/* Find physical locator by VTEP IP */
const struct ovsrec_physical_locator *
ovs_find_physical_locator_by_ip(struct zebra_ns *zns, const struct ipaddr *vtep_ip);

/* Create physical locator for VTEP IP */
const struct ovsrec_physical_locator *
ovs_create_physical_locator(struct ovsdb_idl_txn *txn, const struct ipaddr *vtep_ip);

#ifdef __cplusplus
}
#endif

#endif /* HAVE_OVS */
#endif /* _ZEBRA_KERNEL_OVS_H */
