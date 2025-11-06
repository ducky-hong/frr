// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Zebra userspace event provider implementation
 * Copyright (C) 2025 Free Range Routing
 */

#include "zebra.h"

#include "log.h"
#include "memory.h"
#include "prefix.h"
#include "table.h"
#include "vrf.h"
#include "if.h"
#include "zebra/zebra_router.h"
#include "zebra/zebra_ns.h"
#include "zebra/interface.h"
#include "zebra/zebra_vrf.h"
#include "zebra/zebra_evpn.h"
#include "zebra/zebra_evpn_mac.h"
#include "zebra/zebra_evpn_neigh.h"
#include "zebra/zebra_l2.h"
#include "zebra/zebra_vxlan.h"
#include "zebra/zebra_evpn_mh.h"
#include "zebra/debug.h"
#include "zebra/zebra_usrspace_provider.h"

DEFINE_MTYPE_STATIC(ZEBRA, USRSPACE_PROV, "Userspace Provider");

/* Global state */
static struct {
	bool enabled;
	struct usrspace_provider *active_provider;
	struct list *providers;
} usrspace_state;

/* Forward declarations */
static void usrspace_event_read(struct event *t);

/*
 * Initialize userspace provider subsystem
 */
void zebra_usrspace_provider_init(void)
{
	memset(&usrspace_state, 0, sizeof(usrspace_state));
	usrspace_state.providers = list_new();
	usrspace_state.enabled = false;
	usrspace_state.active_provider = NULL;

	if (IS_ZEBRA_DEBUG_EVENT)
		zlog_debug("%s: Userspace provider subsystem initialized",
			   __func__);
}

/*
 * Shutdown userspace provider subsystem
 */
void zebra_usrspace_provider_fini(void)
{
	struct listnode *node, *nnode;
	struct usrspace_provider *prov;

	if (!usrspace_state.providers)
		return;

	for (ALL_LIST_ELEMENTS(usrspace_state.providers, node, nnode, prov)) {
		if (prov->started && prov->ops->stop)
			prov->ops->stop(prov);

		if (prov->ops->fini)
			prov->ops->fini(prov);

		list_delete_node(usrspace_state.providers, node);
	}

	list_delete(&usrspace_state.providers);
	usrspace_state.active_provider = NULL;

	if (IS_ZEBRA_DEBUG_EVENT)
		zlog_debug("%s: Userspace provider subsystem shut down",
			   __func__);
}

/*
 * Register a provider
 */
int zebra_usrspace_provider_register(struct usrspace_provider *prov)
{
	if (!prov || !prov->ops) {
		zlog_err("%s: Invalid provider", __func__);
		return -1;
	}

	listnode_add(usrspace_state.providers, prov);

	/* If no active provider, make this one active */
	if (!usrspace_state.active_provider) {
		usrspace_state.active_provider = prov;

		/* Set event callback */
		prov->event_cb = NULL;  /* Will be set when started */
		prov->event_ctx = NULL;
	}

	if (IS_ZEBRA_DEBUG_EVENT)
		zlog_debug("%s: Registered userspace provider: %s", __func__,
			   prov->name);

	/* Initialize provider */
	if (prov->ops->init) {
		int ret = prov->ops->init(prov);
		if (ret < 0) {
			zlog_err("%s: Failed to initialize provider %s",
				 __func__, prov->name);
			return ret;
		}
	}

	return 0;
}

/*
 * Unregister a provider
 */
int zebra_usrspace_provider_unregister(struct usrspace_provider *prov)
{
	if (!prov)
		return -1;

	if (prov->started && prov->ops->stop)
		prov->ops->stop(prov);

	if (prov->ops->fini)
		prov->ops->fini(prov);

	listnode_delete(usrspace_state.providers, prov);

	if (usrspace_state.active_provider == prov)
		usrspace_state.active_provider = NULL;

	if (IS_ZEBRA_DEBUG_EVENT)
		zlog_debug("%s: Unregistered userspace provider: %s", __func__,
			   prov->name);

	return 0;
}

/*
 * Enable/disable userspace mode
 */
void zebra_usrspace_set_enabled(bool enabled)
{
	struct usrspace_provider *prov = usrspace_state.active_provider;

	usrspace_state.enabled = enabled;

	if (!prov)
		return;

	if (enabled && !prov->started) {
		/* Start the provider */
		if (prov->ops->start) {
			int ret = prov->ops->start(prov);
			if (ret < 0) {
				zlog_err("%s: Failed to start provider %s",
					 __func__, prov->name);
				return;
			}
		}

		prov->started = true;

		/* Register with event loop */
		if (prov->ops->get_fd && prov->ops->poll) {
			int fd = prov->ops->get_fd(prov);
			if (fd >= 0) {
				event_add_read(zrouter.master,
					      usrspace_event_read, prov, fd,
					      &prov->t_read);
			}
		}

		if (IS_ZEBRA_DEBUG_EVENT)
			zlog_debug("%s: Enabled userspace mode with provider %s",
				   __func__, prov->name);

	} else if (!enabled && prov->started) {
		/* Stop the provider */
		EVENT_OFF(prov->t_read);

		if (prov->ops->stop)
			prov->ops->stop(prov);

		prov->started = false;

		if (IS_ZEBRA_DEBUG_EVENT)
			zlog_debug("%s: Disabled userspace mode", __func__);
	}
}

bool zebra_usrspace_is_enabled(void)
{
	return usrspace_state.enabled;
}

struct usrspace_provider *zebra_usrspace_get_provider(void)
{
	return usrspace_state.active_provider;
}

/*
 * Process interface add event
 */
static int usrspace_process_intf_add(struct usrspace_event *event)
{
	struct usrspace_intf_event *iev = &event->u.intf;
	struct zebra_ns *zns;
	struct interface *ifp;
	struct zebra_if *zif;

	zns = zebra_ns_lookup(event->ns_id);
	if (!zns) {
		zlog_warn("%s: Unknown namespace %u", __func__, event->ns_id);
		return -1;
	}

	/* Check if interface already exists */
	ifp = if_lookup_by_index(iev->ifindex, event->ns_id);
	if (ifp) {
		if (IS_ZEBRA_DEBUG_EVENT)
			zlog_debug("%s: Interface %s already exists",
				   __func__, iev->ifname);
		return 0;
	}

	/* Create interface */
	ifp = if_get_by_name(iev->ifname, event->ns_id, NULL);
	if (!ifp) {
		zlog_err("%s: Failed to create interface %s", __func__,
			 iev->ifname);
		return -1;
	}

	/* Set interface properties */
	ifp->ifindex = iev->ifindex;
	ifp->mtu = iev->mtu;
	ifp->mtu6 = iev->mtu;
	ifp->flags = iev->flags;

	if (iev->hw_addr_len > 0 && iev->hw_addr_len <= ETH_ALEN) {
		memcpy(ifp->hw_addr, iev->hw_addr, iev->hw_addr_len);
		ifp->hw_addr_len = iev->hw_addr_len;
	}

	/* Set zebra interface data */
	zif = (struct zebra_if *)ifp->info;
	if (zif) {
		zif->zif_type = iev->zif_type;
		zif->link_ifindex = iev->link_ifindex;

		if (iev->is_vxlan) {
			struct zebra_l2info_vxlan *vxl = &zif->l2info.vxl;
			vxl->vni = iev->vni;
			vxl->vtep_ip = iev->vtep_ip;
			zif->brslave_info.br_slave = false;
		}

		if (iev->is_bridge) {
			zif->zif_type = ZEBRA_IF_BRIDGE;
		}

		if (iev->is_bridge_member && iev->bridge_ifname[0]) {
			struct interface *br_if;

			br_if = if_lookup_by_name(iev->bridge_ifname,
						  event->ns_id);
			if (br_if) {
				zif->brslave_info.br_slave = true;
				zif->brslave_info.bridge_ifindex =
					br_if->ifindex;
			}
		}

		if (iev->is_bond) {
			zif->zif_type = ZEBRA_IF_BOND;
		}

		if (iev->is_vlan) {
			struct zebra_l2info_vlan *vlan = &zif->l2info.vl;
			vlan->vid = iev->vlan_id;
		}
	}

	/* Notify zebra core */
	if_nbr_ipv6ll_to_ipv4ll_neigh_update(ifp, &ifp->ll_ip6, true);

	if (IS_ZEBRA_DEBUG_EVENT)
		zlog_debug("%s: Added interface %s ifindex %u", __func__,
			   iev->ifname, iev->ifindex);

	return 0;
}

/*
 * Process interface delete event
 */
static int usrspace_process_intf_delete(struct usrspace_event *event)
{
	struct usrspace_intf_event *iev = &event->u.intf;
	struct interface *ifp;

	ifp = if_lookup_by_index(iev->ifindex, event->ns_id);
	if (!ifp) {
		if (IS_ZEBRA_DEBUG_EVENT)
			zlog_debug("%s: Interface ifindex %u not found",
				   __func__, iev->ifindex);
		return 0;
	}

	if (IS_ZEBRA_DEBUG_EVENT)
		zlog_debug("%s: Deleting interface %s ifindex %u", __func__,
			   ifp->name, iev->ifindex);

	if_delete(&ifp);

	return 0;
}

/*
 * Process interface up event
 */
static int usrspace_process_intf_up(struct usrspace_event *event)
{
	struct usrspace_intf_event *iev = &event->u.intf;
	struct interface *ifp;

	ifp = if_lookup_by_index(iev->ifindex, event->ns_id);
	if (!ifp) {
		zlog_warn("%s: Interface ifindex %u not found", __func__,
			  iev->ifindex);
		return -1;
	}

	if (IS_ZEBRA_DEBUG_EVENT)
		zlog_debug("%s: Interface %s is up", __func__, ifp->name);

	if_set_flags(ifp, IFF_UP | IFF_RUNNING);
	if_refresh(ifp);

	return 0;
}

/*
 * Process interface down event
 */
static int usrspace_process_intf_down(struct usrspace_event *event)
{
	struct usrspace_intf_event *iev = &event->u.intf;
	struct interface *ifp;

	ifp = if_lookup_by_index(iev->ifindex, event->ns_id);
	if (!ifp) {
		zlog_warn("%s: Interface ifindex %u not found", __func__,
			  iev->ifindex);
		return -1;
	}

	if (IS_ZEBRA_DEBUG_EVENT)
		zlog_debug("%s: Interface %s is down", __func__, ifp->name);

	if_unset_flags(ifp, IFF_UP | IFF_RUNNING);
	if_refresh(ifp);

	return 0;
}

/*
 * Process interface address add event
 */
static int usrspace_process_intf_addr_add(struct usrspace_event *event)
{
	struct usrspace_addr_event *aev = &event->u.addr;
	struct interface *ifp;
	struct connected *ifc;
	struct prefix *addr;

	ifp = if_lookup_by_index(aev->ifindex, event->ns_id);
	if (!ifp) {
		zlog_warn("%s: Interface ifindex %u not found", __func__,
			  aev->ifindex);
		return -1;
	}

	/* Check if address already exists */
	ifc = connected_check_ptp(ifp, &aev->addr, &aev->dest);
	if (ifc) {
		if (IS_ZEBRA_DEBUG_EVENT)
			zlog_debug("%s: Address already exists on %s",
				   __func__, ifp->name);
		return 0;
	}

	/* Add address */
	addr = prefix_new();
	prefix_copy(addr, &aev->addr);

	ifc = connected_add_by_prefix(ifp, addr, NULL);
	if (aev->label[0])
		strlcpy(ifc->label, aev->label, sizeof(ifc->label));

	if (IS_ZEBRA_DEBUG_EVENT)
		zlog_debug("%s: Added address %pFX to interface %s", __func__,
			   &aev->addr, ifp->name);

	/* Notify zebra core */
	if (aev->family == AF_INET)
		zebra_interface_address_add_update(ifp, ifc);
	else
		zebra_interface_address_add_update(ifp, ifc);

	return 0;
}

/*
 * Process interface address delete event
 */
static int usrspace_process_intf_addr_del(struct usrspace_event *event)
{
	struct usrspace_addr_event *aev = &event->u.addr;
	struct interface *ifp;
	struct connected *ifc;

	ifp = if_lookup_by_index(aev->ifindex, event->ns_id);
	if (!ifp) {
		zlog_warn("%s: Interface ifindex %u not found", __func__,
			  aev->ifindex);
		return -1;
	}

	/* Find and delete address */
	ifc = connected_check_ptp(ifp, &aev->addr, &aev->dest);
	if (!ifc) {
		if (IS_ZEBRA_DEBUG_EVENT)
			zlog_debug("%s: Address not found on %s", __func__,
				   ifp->name);
		return 0;
	}

	if (IS_ZEBRA_DEBUG_EVENT)
		zlog_debug("%s: Deleted address %pFX from interface %s",
			   __func__, &aev->addr, ifp->name);

	/* Notify zebra core */
	zebra_interface_address_delete_update(ifp, ifc);

	connected_delete_by_prefix(ifp, &aev->addr);

	return 0;
}

/*
 * Process MAC add event
 */
static int usrspace_process_mac_add(struct usrspace_event *event)
{
	struct usrspace_mac_event *mev = &event->u.mac;
	struct interface *ifp;
	struct zebra_if *zif;
	struct zebra_evpn *zevpn;
	struct zebra_mac *mac;
	struct ethaddr macaddr;

	ifp = if_lookup_by_index(mev->ifindex, event->ns_id);
	if (!ifp) {
		zlog_warn("%s: Interface ifindex %u not found", __func__,
			  mev->ifindex);
		return -1;
	}

	zif = (struct zebra_if *)ifp->info;
	if (!zif) {
		zlog_warn("%s: No zebra_if for interface %s", __func__,
			  ifp->name);
		return -1;
	}

	/* Get or create EVPN */
	zevpn = zebra_evpn_lookup(mev->vni);
	if (!zevpn) {
		zevpn = zebra_evpn_add(mev->vni);
		if (!zevpn) {
			zlog_err("%s: Failed to create EVPN for VNI %u",
				 __func__, mev->vni);
			return -1;
		}
	}

	memcpy(&macaddr.octet, mev->mac, ETH_ALEN);

	/* Get or create MAC */
	mac = zebra_evpn_mac_lookup(zevpn, &macaddr);
	if (!mac) {
		mac = zebra_evpn_mac_add(zevpn, &macaddr);
		if (!mac) {
			zlog_err("%s: Failed to add MAC %pEA", __func__,
				 &macaddr);
			return -1;
		}
	}

	/* Update MAC properties */
	if (mev->is_local) {
		zebra_evpn_mac_clear_fwd_info(mac);
		mac->fwd_info.local.ifindex = mev->ifindex;
		mac->fwd_info.local.vid = mev->vid;

		if (mev->esi_valid) {
			struct zebra_evpn_es *es = zebra_evpn_es_find(&mev->esi);
			if (es) {
				mac->es = es;
				SET_FLAG(mac->flags, ZEBRA_MAC_ES_PEER_ACTIVE);
			}
		}

		SET_FLAG(mac->flags, ZEBRA_MAC_LOCAL);
	} else {
		/* Remote MAC */
		mac->fwd_info.r_vtep_ip = mev->ip;
		UNSET_FLAG(mac->flags, ZEBRA_MAC_LOCAL);
		SET_FLAG(mac->flags, ZEBRA_MAC_REMOTE);
	}

	if (mev->is_static)
		SET_FLAG(mac->flags, ZEBRA_MAC_STICKY);

	if (IS_ZEBRA_DEBUG_EVPN_MH_MAC || IS_ZEBRA_DEBUG_VXLAN)
		zlog_debug("%s: Added MAC %pEA VNI %u ifindex %u",
			   __func__, &macaddr, mev->vni,
			   mev->ifindex);

	return 0;
}

/*
 * Process MAC delete event
 */
static int usrspace_process_mac_del(struct usrspace_event *event)
{
	struct usrspace_mac_event *mev = &event->u.mac;
	struct zebra_evpn *zevpn;
	struct zebra_mac *mac;
	struct ethaddr macaddr;

	zevpn = zebra_evpn_lookup(mev->vni);
	if (!zevpn) {
		if (IS_ZEBRA_DEBUG_VXLAN)
			zlog_debug("%s: EVPN VNI %u not found", __func__,
				   mev->vni);
		return 0;
	}

	memcpy(&macaddr.octet, mev->mac, ETH_ALEN);

	mac = zebra_evpn_mac_lookup(zevpn, &macaddr);
	if (!mac) {
		if (IS_ZEBRA_DEBUG_VXLAN)
			zlog_debug("%s: MAC %pEA not found in VNI %u",
				   __func__, &macaddr, mev->vni);
		return 0;
	}

	if (IS_ZEBRA_DEBUG_EVPN_MH_MAC || IS_ZEBRA_DEBUG_VXLAN)
		zlog_debug("%s: Deleted MAC %pEA VNI %u", __func__,
			   &macaddr, mev->vni);

	zebra_evpn_mac_del(zevpn, mac);

	return 0;
}

/*
 * Process neighbor add/update event
 */
static int usrspace_process_neigh_add(struct usrspace_event *event)
{
	struct usrspace_neigh_event *nev = &event->u.neigh;
	struct interface *ifp;

	ifp = if_lookup_by_index(nev->ifindex, event->ns_id);
	if (!ifp) {
		zlog_warn("%s: Interface ifindex %u not found", __func__,
			  nev->ifindex);
		return -1;
	}

	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("%s: Added neighbor %pIA on interface %s", __func__,
			   &nev->ip, ifp->name);

	/* TODO: Process EVPN neighbors */

	return 0;
}

/*
 * Process neighbor delete event
 */
static int usrspace_process_neigh_del(struct usrspace_event *event)
{
	struct usrspace_neigh_event *nev = &event->u.neigh;
	struct interface *ifp;

	ifp = if_lookup_by_index(nev->ifindex, event->ns_id);
	if (!ifp) {
		zlog_warn("%s: Interface ifindex %u not found", __func__,
			  nev->ifindex);
		return -1;
	}

	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("%s: Deleted neighbor %pIA from interface %s",
			   __func__, &nev->ip, ifp->name);

	/* TODO: Process EVPN neighbors */

	return 0;
}

/*
 * Process an injected event
 */
static int usrspace_process_event(struct usrspace_event *event)
{
	int ret = 0;

	switch (event->type) {
	case USRSPACE_EVENT_INTF_ADD:
		ret = usrspace_process_intf_add(event);
		break;
	case USRSPACE_EVENT_INTF_DELETE:
		ret = usrspace_process_intf_delete(event);
		break;
	case USRSPACE_EVENT_INTF_UP:
		ret = usrspace_process_intf_up(event);
		break;
	case USRSPACE_EVENT_INTF_DOWN:
		ret = usrspace_process_intf_down(event);
		break;
	case USRSPACE_EVENT_INTF_ADDR_ADD:
		ret = usrspace_process_intf_addr_add(event);
		break;
	case USRSPACE_EVENT_INTF_ADDR_DEL:
		ret = usrspace_process_intf_addr_del(event);
		break;
	case USRSPACE_EVENT_MAC_ADD:
		ret = usrspace_process_mac_add(event);
		break;
	case USRSPACE_EVENT_MAC_DEL:
		ret = usrspace_process_mac_del(event);
		break;
	case USRSPACE_EVENT_NEIGH_ADD:
	case USRSPACE_EVENT_NEIGH_UPDATE:
		ret = usrspace_process_neigh_add(event);
		break;
	case USRSPACE_EVENT_NEIGH_DEL:
		ret = usrspace_process_neigh_del(event);
		break;
	default:
		zlog_warn("%s: Unknown event type %d", __func__, event->type);
		ret = -1;
		break;
	}

	return ret;
}

/*
 * Inject an event into zebra
 */
int zebra_usrspace_inject_event(struct usrspace_event *event)
{
	if (!usrspace_state.enabled) {
		if (IS_ZEBRA_DEBUG_EVENT)
			zlog_debug("%s: Userspace mode not enabled", __func__);
		return -1;
	}

	return usrspace_process_event(event);
}

/*
 * Event loop callback - read events from provider
 */
static void usrspace_event_read(struct event *t)
{
	struct usrspace_provider *prov = EVENT_ARG(t);
	int fd = EVENT_FD(t);

	prov->t_read = NULL;

	if (!prov->started) {
		if (IS_ZEBRA_DEBUG_EVENT)
			zlog_debug("%s: Provider not started", __func__);
		return;
	}

	/* Poll for events */
	if (prov->ops->poll)
		prov->ops->poll(prov);

	/* Re-register for next event */
	event_add_read(zrouter.master, usrspace_event_read, prov, fd,
		      &prov->t_read);
}

/*
 * Helper functions to create and inject specific events
 */

int zebra_usrspace_inject_intf_add(ns_id_t ns_id, const char *ifname,
				   ifindex_t ifindex, uint32_t mtu,
				   const uint8_t *hw_addr)
{
	struct usrspace_event event;

	memset(&event, 0, sizeof(event));
	event.type = USRSPACE_EVENT_INTF_ADD;
	event.ns_id = ns_id;

	strlcpy(event.u.intf.ifname, ifname, sizeof(event.u.intf.ifname));
	event.u.intf.ifindex = ifindex;
	event.u.intf.mtu = mtu;
	event.u.intf.flags = 0;
	event.u.intf.zif_type = ZEBRA_IF_OTHER;
	event.u.intf.vrf_id = VRF_DEFAULT;

	if (hw_addr) {
		memcpy(event.u.intf.hw_addr, hw_addr, ETH_ALEN);
		event.u.intf.hw_addr_len = ETH_ALEN;
	}

	return zebra_usrspace_inject_event(&event);
}

int zebra_usrspace_inject_intf_delete(ns_id_t ns_id, ifindex_t ifindex)
{
	struct usrspace_event event;

	memset(&event, 0, sizeof(event));
	event.type = USRSPACE_EVENT_INTF_DELETE;
	event.ns_id = ns_id;
	event.u.intf.ifindex = ifindex;

	return zebra_usrspace_inject_event(&event);
}

int zebra_usrspace_inject_intf_up(ns_id_t ns_id, ifindex_t ifindex)
{
	struct usrspace_event event;

	memset(&event, 0, sizeof(event));
	event.type = USRSPACE_EVENT_INTF_UP;
	event.ns_id = ns_id;
	event.u.intf.ifindex = ifindex;

	return zebra_usrspace_inject_event(&event);
}

int zebra_usrspace_inject_intf_down(ns_id_t ns_id, ifindex_t ifindex)
{
	struct usrspace_event event;

	memset(&event, 0, sizeof(event));
	event.type = USRSPACE_EVENT_INTF_DOWN;
	event.ns_id = ns_id;
	event.u.intf.ifindex = ifindex;

	return zebra_usrspace_inject_event(&event);
}

int zebra_usrspace_inject_intf_addr_add(ns_id_t ns_id, ifindex_t ifindex,
					int family, const struct prefix *addr)
{
	struct usrspace_event event;

	memset(&event, 0, sizeof(event));
	event.type = USRSPACE_EVENT_INTF_ADDR_ADD;
	event.ns_id = ns_id;
	event.u.addr.ifindex = ifindex;
	event.u.addr.family = family;
	prefix_copy(&event.u.addr.addr, addr);

	return zebra_usrspace_inject_event(&event);
}

int zebra_usrspace_inject_intf_addr_del(ns_id_t ns_id, ifindex_t ifindex,
					int family, const struct prefix *addr)
{
	struct usrspace_event event;

	memset(&event, 0, sizeof(event));
	event.type = USRSPACE_EVENT_INTF_ADDR_DEL;
	event.ns_id = ns_id;
	event.u.addr.ifindex = ifindex;
	event.u.addr.family = family;
	prefix_copy(&event.u.addr.addr, addr);

	return zebra_usrspace_inject_event(&event);
}

int zebra_usrspace_inject_mac_add(ns_id_t ns_id, ifindex_t ifindex,
				  uint32_t vni, const uint8_t *mac,
				  vlanid_t vid, bool is_local,
				  const struct ipaddr *vtep_ip,
				  const esi_t *esi)
{
	struct usrspace_event event;

	memset(&event, 0, sizeof(event));
	event.type = USRSPACE_EVENT_MAC_ADD;
	event.ns_id = ns_id;
	event.u.mac.ifindex = ifindex;
	event.u.mac.vni = vni;
	memcpy(event.u.mac.mac, mac, ETH_ALEN);
	event.u.mac.vid = vid;
	event.u.mac.is_local = is_local;

	if (vtep_ip)
		event.u.mac.ip = *vtep_ip;

	if (esi) {
		event.u.mac.esi = *esi;
		event.u.mac.esi_valid = true;
	}

	return zebra_usrspace_inject_event(&event);
}

int zebra_usrspace_inject_mac_del(ns_id_t ns_id, ifindex_t ifindex,
				  uint32_t vni, const uint8_t *mac,
				  vlanid_t vid)
{
	struct usrspace_event event;

	memset(&event, 0, sizeof(event));
	event.type = USRSPACE_EVENT_MAC_DEL;
	event.ns_id = ns_id;
	event.u.mac.ifindex = ifindex;
	event.u.mac.vni = vni;
	memcpy(event.u.mac.mac, mac, ETH_ALEN);
	event.u.mac.vid = vid;

	return zebra_usrspace_inject_event(&event);
}

int zebra_usrspace_inject_neigh_add(ns_id_t ns_id, ifindex_t ifindex,
				    int family, const struct ipaddr *ip,
				    const uint8_t *mac, uint16_t state,
				    uint32_t flags)
{
	struct usrspace_event event;

	memset(&event, 0, sizeof(event));
	event.type = USRSPACE_EVENT_NEIGH_ADD;
	event.ns_id = ns_id;
	event.u.neigh.ifindex = ifindex;
	event.u.neigh.family = family;
	event.u.neigh.ip = *ip;
	memcpy(event.u.neigh.mac, mac, ETH_ALEN);
	event.u.neigh.state = state;
	event.u.neigh.flags = flags;

	return zebra_usrspace_inject_event(&event);
}

int zebra_usrspace_inject_neigh_del(ns_id_t ns_id, ifindex_t ifindex,
				    int family, const struct ipaddr *ip)
{
	struct usrspace_event event;

	memset(&event, 0, sizeof(event));
	event.type = USRSPACE_EVENT_NEIGH_DEL;
	event.ns_id = ns_id;
	event.u.neigh.ifindex = ifindex;
	event.u.neigh.family = family;
	event.u.neigh.ip = *ip;

	return zebra_usrspace_inject_event(&event);
}

/*
 * Configuration
 */
void zebra_usrspace_provider_config_write(struct vty *vty)
{
	if (usrspace_state.enabled) {
		vty_out(vty, "userspace-dataplane\n");

		if (usrspace_state.active_provider)
			vty_out(vty, " provider %s\n",
				usrspace_state.active_provider->name);
	}
}
