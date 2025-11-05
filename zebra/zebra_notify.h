// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Zebra Notification Provider API
 * Allows pluggable notification sources (kernel, userspace, etc.)
 * to inject interface/FDB/ARP events into zebra
 *
 * Copyright (C) 2025 FRR Community
 */

#ifndef _ZEBRA_NOTIFY_H
#define _ZEBRA_NOTIFY_H

#include <zebra.h>
#include "lib/if.h"
#include "lib/vxlan.h"
#include "lib/prefix.h"
#include "zebra/zebra_evpn_mh.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Notification operation types - events from dataplane to zebra */
enum zebra_notify_op {
	NOTIFY_OP_NONE = 0,

	/* Interface notifications */
	NOTIFY_OP_INTF_ADD,        /* Interface discovered */
	NOTIFY_OP_INTF_UPDATE,     /* Interface state change */
	NOTIFY_OP_INTF_DELETE,     /* Interface removed */

	/* FDB/MAC notifications - critical for EVPN */
	NOTIFY_OP_FDB_ADD,         /* MAC learned on bridge */
	NOTIFY_OP_FDB_DELETE,      /* MAC expired/removed */

	/* Neighbor/ARP notifications */
	NOTIFY_OP_NEIGH_ADD,       /* ARP/ND learned */
	NOTIFY_OP_NEIGH_DELETE,    /* Neighbor expired */

	/* VLAN notifications */
	NOTIFY_OP_VLAN_UPDATE,     /* VLAN membership change */
};

/* Notification context - carries event data from provider to zebra */
struct zebra_notify_ctx {
	enum zebra_notify_op op;
	ns_id_t ns_id;

	/* Interface information */
	char ifname[IFNAMSIZ];
	ifindex_t ifindex;
	uint32_t flags;              /* IFF_UP, IFF_RUNNING, etc. */
	uint32_t mtu;
	uint8_t hw_addr[ETH_ALEN];

	/* Bridge/VLAN info */
	ifindex_t bridge_ifindex;
	vlanid_t vid;

	/* FDB/MAC information (for EVPN) */
	struct ethaddr mac;
	vni_t vni;
	bool is_static;
	bool is_local;               /* Local vs remote MAC */

	/* Neighbor/ARP information */
	struct ipaddr ip;
	int family;                  /* AF_INET or AF_INET6 */

	/* EVPN Multihoming - ESI information */
	esi_t esi;
	bool has_esi;
};

/* Notification provider structure */
struct zebra_notify_provider {
	char np_name[64];
	uint32_t np_id;

	/* Provider callbacks */
	int (*np_start)(struct zebra_notify_provider *prov);
	int (*np_poll)(struct zebra_notify_provider *prov);
	int (*np_stop)(struct zebra_notify_provider *prov);

	/* Provider private data */
	void *np_data;

	/* Statistics */
	uint64_t np_events_sent;
	uint64_t np_errors;
};

/* Initialize notification provider subsystem */
void zebra_notify_init(void);

/* Register a notification provider */
int zebra_notify_provider_register(
	const char *name,
	int (*start_fp)(struct zebra_notify_provider *prov),
	int (*poll_fp)(struct zebra_notify_provider *prov),
	int (*stop_fp)(struct zebra_notify_provider *prov),
	void *data,
	struct zebra_notify_provider **prov_out);

/* Unregister a notification provider */
void zebra_notify_provider_unregister(struct zebra_notify_provider *prov);

/* API for providers to inject notifications into zebra */
int zebra_notify_inject(struct zebra_notify_ctx *ctx);

/* Allocate/free notification context */
struct zebra_notify_ctx *zebra_notify_ctx_alloc(void);
void zebra_notify_ctx_free(struct zebra_notify_ctx **ctx);

/* Start notification polling (call after providers are registered) */
void zebra_notify_start_poll(void);

/* Cleanup on shutdown */
void zebra_notify_shutdown(void);

/* Helper to convert op to string */
const char *zebra_notify_op2str(enum zebra_notify_op op);

#ifdef __cplusplus
}
#endif

#endif /* _ZEBRA_NOTIFY_H */
