// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Zebra userspace event provider API
 * Copyright (C) 2025 Free Range Routing
 *
 * This provides an abstraction for receiving interface, FDB, and ARP/neighbor
 * events from userspace instead of the kernel. This enables running FRR
 * in environments without kernel dependencies, such as testing and simulation.
 */

#ifndef _ZEBRA_USRSPACE_PROVIDER_H
#define _ZEBRA_USRSPACE_PROVIDER_H

#include "zebra.h"
#include "prefix.h"
#include "if.h"
#include "zebra_dplane.h"
#include "zebra/interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Event types that can be injected from userspace */
enum usrspace_event_type {
	/* Interface events */
	USRSPACE_EVENT_INTF_ADD,
	USRSPACE_EVENT_INTF_DELETE,
	USRSPACE_EVENT_INTF_UP,
	USRSPACE_EVENT_INTF_DOWN,
	USRSPACE_EVENT_INTF_ADDR_ADD,
	USRSPACE_EVENT_INTF_ADDR_DEL,

	/* MAC/FDB events */
	USRSPACE_EVENT_MAC_ADD,
	USRSPACE_EVENT_MAC_DEL,

	/* Neighbor/ARP events */
	USRSPACE_EVENT_NEIGH_ADD,
	USRSPACE_EVENT_NEIGH_UPDATE,
	USRSPACE_EVENT_NEIGH_DEL,

	/* Bridge port events */
	USRSPACE_EVENT_BR_PORT_UPDATE,

	/* VLAN events */
	USRSPACE_EVENT_VLAN_ADD,
	USRSPACE_EVENT_VLAN_DEL,
};

/* Interface event data */
struct usrspace_intf_event {
	char ifname[IFNAMSIZ];
	ifindex_t ifindex;
	uint32_t mtu;
	uint32_t flags;  /* IFF_UP, IFF_RUNNING, etc. */
	uint8_t hw_addr[ETH_ALEN];
	uint32_t hw_addr_len;

	/* Interface type */
	enum zebra_iftype zif_type;

	/* VRF */
	vrf_id_t vrf_id;

	/* Bond/LAG info */
	bool is_bond;
	bool is_bond_member;
	char bond_ifname[IFNAMSIZ];

	/* Bridge info */
	bool is_bridge;
	bool is_bridge_member;
	char bridge_ifname[IFNAMSIZ];

	/* VXLAN info */
	bool is_vxlan;
	uint32_t vni;
	struct in_addr vtep_ip;

	/* Link info */
	ifindex_t link_ifindex;
	char link_ifname[IFNAMSIZ];

	/* VLAN info */
	bool is_vlan;
	uint16_t vlan_id;
};

/* Address event data */
struct usrspace_addr_event {
	ifindex_t ifindex;
	int family;
	struct prefix addr;
	struct prefix dest; /* peer address for P2P */
	char label[64];
	uint32_t flags;
};

/* MAC/FDB event data */
struct usrspace_mac_event {
	ifindex_t ifindex;
	uint32_t vni;
	uint8_t mac[ETH_ALEN];
	struct ipaddr ip; /* for remote MACs */
	vlanid_t vid;

	/* Flags */
	bool is_local;
	bool is_static;
	bool is_sticky;
	bool is_router;

	/* ESI for multihoming */
	esi_t esi;
	bool esi_valid;
};

/* Neighbor/ARP event data */
struct usrspace_neigh_event {
	ifindex_t ifindex;
	int family;
	struct ipaddr ip;
	uint8_t mac[ETH_ALEN];
	uint32_t flags; /* DPLANE_NTF_* */
	uint16_t state; /* DPLANE_NUD_* */

	/* EVPN info */
	bool is_ext;
	bool is_router;
};

/* Bridge port event data */
struct usrspace_br_port_event {
	ifindex_t ifindex;
	ifindex_t br_ifindex;
	uint32_t sph_filter_cnt;
	struct in_addr sph_filters[ES_VTEP_MAX_CNT];
	uint32_t flags; /* DPLANE_BR_PORT_NON_DF */
};

/* VLAN event data */
struct usrspace_vlan_event {
	ifindex_t ifindex;
	uint16_t vid;
	bool is_pvid;
	bool is_untagged;
};

/* Generic event structure */
struct usrspace_event {
	enum usrspace_event_type type;
	ns_id_t ns_id;

	union {
		struct usrspace_intf_event intf;
		struct usrspace_addr_event addr;
		struct usrspace_mac_event mac;
		struct usrspace_neigh_event neigh;
		struct usrspace_br_port_event br_port;
		struct usrspace_vlan_event vlan;
	} u;
};

/* Forward declaration */
struct usrspace_provider;

/* Provider operations - implementation provides these */
struct usrspace_provider_ops {
	/* Initialize the provider */
	int (*init)(struct usrspace_provider *prov);

	/* Shutdown the provider */
	int (*fini)(struct usrspace_provider *prov);

	/* Start listening for events */
	int (*start)(struct usrspace_provider *prov);

	/* Stop listening for events */
	int (*stop)(struct usrspace_provider *prov);

	/* Get file descriptor for event loop (optional) */
	int (*get_fd)(struct usrspace_provider *prov);

	/* Poll for events (called by event loop) */
	int (*poll)(struct usrspace_provider *prov);
};

/* Provider instance */
struct usrspace_provider {
	const char *name;
	const struct usrspace_provider_ops *ops;
	void *priv; /* Provider private data */

	bool enabled;
	bool started;

	/* Event callback - called when event is injected */
	int (*event_cb)(struct usrspace_event *event, void *ctx);
	void *event_ctx;

	/* Event thread */
	struct event *t_read;
};

/*
 * Core API
 */

/* Initialize userspace provider subsystem */
void zebra_usrspace_provider_init(void);

/* Shutdown userspace provider subsystem */
void zebra_usrspace_provider_fini(void);

/* Register a provider */
int zebra_usrspace_provider_register(struct usrspace_provider *prov);

/* Unregister a provider */
int zebra_usrspace_provider_unregister(struct usrspace_provider *prov);

/* Enable/disable userspace mode */
void zebra_usrspace_set_enabled(bool enabled);
bool zebra_usrspace_is_enabled(void);

/* Get the active provider */
struct usrspace_provider *zebra_usrspace_get_provider(void);

/*
 * Event injection API - used by providers to inject events
 */

/* Inject an event into zebra */
int zebra_usrspace_inject_event(struct usrspace_event *event);

/* Helper functions to create and inject specific events */
int zebra_usrspace_inject_intf_add(ns_id_t ns_id, const char *ifname,
				   ifindex_t ifindex, uint32_t mtu,
				   const uint8_t *hw_addr);

int zebra_usrspace_inject_intf_delete(ns_id_t ns_id, ifindex_t ifindex);

int zebra_usrspace_inject_intf_up(ns_id_t ns_id, ifindex_t ifindex);

int zebra_usrspace_inject_intf_down(ns_id_t ns_id, ifindex_t ifindex);

int zebra_usrspace_inject_intf_addr_add(ns_id_t ns_id, ifindex_t ifindex,
					int family, const struct prefix *addr);

int zebra_usrspace_inject_intf_addr_del(ns_id_t ns_id, ifindex_t ifindex,
					int family, const struct prefix *addr);

int zebra_usrspace_inject_mac_add(ns_id_t ns_id, ifindex_t ifindex,
				  uint32_t vni, const uint8_t *mac,
				  vlanid_t vid, bool is_local,
				  const struct ipaddr *vtep_ip,
				  const esi_t *esi);

int zebra_usrspace_inject_mac_del(ns_id_t ns_id, ifindex_t ifindex,
				  uint32_t vni, const uint8_t *mac,
				  vlanid_t vid);

int zebra_usrspace_inject_neigh_add(ns_id_t ns_id, ifindex_t ifindex,
				    int family, const struct ipaddr *ip,
				    const uint8_t *mac, uint16_t state,
				    uint32_t flags);

int zebra_usrspace_inject_neigh_del(ns_id_t ns_id, ifindex_t ifindex,
				    int family, const struct ipaddr *ip);

/*
 * Configuration
 */
void zebra_usrspace_provider_config_write(struct vty *vty);

#ifdef __cplusplus
}
#endif

#endif /* _ZEBRA_USRSPACE_PROVIDER_H */
