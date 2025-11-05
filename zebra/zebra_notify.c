// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Zebra Notification Provider Implementation
 * Provides pluggable notification infrastructure for EVPN and other features
 *
 * Copyright (C) 2025 FRR Community
 */

#include <zebra.h>
#include "lib/memory.h"
#include "lib/queue.h"
#include "lib/log.h"
#include "lib/frrevent.h"
#include "zebra/zebra_notify.h"
#include "zebra/zebra_evpn.h"
#include "zebra/zebra_evpn_mac.h"
#include "zebra/zebra_evpn_neigh.h"
#include "zebra/interface.h"
#include "zebra/zebra_router.h"

DEFINE_MTYPE_STATIC(ZEBRA, NOTIFY_CTX, "Notification Context");
DEFINE_MTYPE_STATIC(ZEBRA, NOTIFY_PROV, "Notification Provider");

/* Global notification provider list */
static struct zebra_notify_provider *notify_providers[8];
static uint32_t notify_provider_count = 0;
static uint32_t notify_provider_id_gen = 0;

/* Statistics */
static uint64_t notify_total_events = 0;
static uint64_t notify_fdb_events = 0;
static uint64_t notify_intf_events = 0;
static uint64_t notify_neigh_events = 0;

/* Forward declarations */
static int zebra_notify_process_fdb(struct zebra_notify_ctx *ctx);
static int zebra_notify_process_intf(struct zebra_notify_ctx *ctx);
static int zebra_notify_process_neigh(struct zebra_notify_ctx *ctx);

/* Convert operation to string */
const char *zebra_notify_op2str(enum zebra_notify_op op)
{
	switch (op) {
	case NOTIFY_OP_NONE:
		return "NONE";
	case NOTIFY_OP_INTF_ADD:
		return "INTF_ADD";
	case NOTIFY_OP_INTF_UPDATE:
		return "INTF_UPDATE";
	case NOTIFY_OP_INTF_DELETE:
		return "INTF_DELETE";
	case NOTIFY_OP_FDB_ADD:
		return "FDB_ADD";
	case NOTIFY_OP_FDB_DELETE:
		return "FDB_DELETE";
	case NOTIFY_OP_NEIGH_ADD:
		return "NEIGH_ADD";
	case NOTIFY_OP_NEIGH_DELETE:
		return "NEIGH_DELETE";
	case NOTIFY_OP_VLAN_UPDATE:
		return "VLAN_UPDATE";
	}
	return "UNKNOWN";
}

/* Initialize notification subsystem */
void zebra_notify_init(void)
{
	zlog_info("Zebra notification provider subsystem initialized");
}

/* Allocate notification context */
struct zebra_notify_ctx *zebra_notify_ctx_alloc(void)
{
	struct zebra_notify_ctx *ctx;

	ctx = XCALLOC(MTYPE_NOTIFY_CTX, sizeof(struct zebra_notify_ctx));
	return ctx;
}

/* Free notification context */
void zebra_notify_ctx_free(struct zebra_notify_ctx **ctx)
{
	if (!ctx || !*ctx)
		return;

	XFREE(MTYPE_NOTIFY_CTX, *ctx);
	*ctx = NULL;
}

/* Register a notification provider */
int zebra_notify_provider_register(
	const char *name,
	int (*start_fp)(struct zebra_notify_provider *prov),
	int (*poll_fp)(struct zebra_notify_provider *prov),
	int (*stop_fp)(struct zebra_notify_provider *prov),
	void *data,
	struct zebra_notify_provider **prov_out)
{
	struct zebra_notify_provider *prov;

	if (!name || !poll_fp) {
		zlog_err("Invalid notification provider registration");
		return -1;
	}

	if (notify_provider_count >= array_size(notify_providers)) {
		zlog_err("Too many notification providers");
		return -1;
	}

	prov = XCALLOC(MTYPE_NOTIFY_PROV,
		       sizeof(struct zebra_notify_provider));

	strlcpy(prov->np_name, name, sizeof(prov->np_name));
	prov->np_id = ++notify_provider_id_gen;
	prov->np_start = start_fp;
	prov->np_poll = poll_fp;
	prov->np_stop = stop_fp;
	prov->np_data = data;

	notify_providers[notify_provider_count++] = prov;

	if (prov_out)
		*prov_out = prov;

	zlog_info("Registered notification provider: %s (id=%u)", name,
		  prov->np_id);

	/* Call start callback if provided */
	if (prov->np_start)
		prov->np_start(prov);

	return 0;
}

/* Unregister notification provider */
void zebra_notify_provider_unregister(struct zebra_notify_provider *prov)
{
	uint32_t i;

	if (!prov)
		return;

	/* Call stop callback */
	if (prov->np_stop)
		prov->np_stop(prov);

	/* Remove from array */
	for (i = 0; i < notify_provider_count; i++) {
		if (notify_providers[i] == prov) {
			/* Shift remaining providers */
			for (uint32_t j = i; j < notify_provider_count - 1;
			     j++) {
				notify_providers[j] = notify_providers[j + 1];
			}
			notify_provider_count--;
			break;
		}
	}

	zlog_info("Unregistered notification provider: %s", prov->np_name);
	XFREE(MTYPE_NOTIFY_PROV, prov);
}

/* Process FDB notification */
static int zebra_notify_process_fdb(struct zebra_notify_ctx *ctx)
{
	struct interface *ifp;
	struct zebra_if *zif;
	struct zebra_evpn *zevpn;
	struct zebra_mac *mac;
	char mac_buf[ETHER_ADDR_STRLEN];
	char esi_buf[ESI_STR_LEN];

	notify_fdb_events++;

	/* Get interface */
	ifp = if_lookup_by_index(ctx->ifindex, ctx->ns_id);
	if (!ifp) {
		zlog_debug("notify: FDB event on unknown interface %u",
			   ctx->ifindex);
		return -1;
	}

	zif = ifp->info;
	if (!zif) {
		zlog_debug("notify: FDB event on interface %s with no zebra_if",
			   ifp->name);
		return -1;
	}

	/* Find EVPN for this VNI */
	zevpn = zebra_evpn_lookup(ctx->vni);
	if (!zevpn) {
		zlog_debug("notify: FDB event for unknown VNI %u", ctx->vni);
		/* This is not necessarily an error - VNI might not be configured yet */
		return 0;
	}

	prefix_mac2str(&ctx->mac, mac_buf, sizeof(mac_buf));

	if (ctx->has_esi)
		esi_to_str(&ctx->esi, esi_buf, sizeof(esi_buf));
	else
		snprintf(esi_buf, sizeof(esi_buf), "none");

	if (ctx->op == NOTIFY_OP_FDB_ADD) {
		zlog_debug(
			"notify: FDB_ADD mac=%s vni=%u ifindex=%u(%s) vid=%u esi=%s local=%s static=%s",
			mac_buf, ctx->vni, ctx->ifindex, ifp->name, ctx->vid,
			esi_buf, ctx->is_local ? "yes" : "no",
			ctx->is_static ? "yes" : "no");

		/* Look up or create MAC entry */
		mac = zebra_evpn_mac_lookup(zevpn, &ctx->mac);
		if (!mac) {
			mac = zebra_evpn_mac_add(zevpn, &ctx->mac);
			if (!mac) {
				zlog_err("notify: Failed to add MAC %s",
					 mac_buf);
				return -1;
			}
		}

		/* Update MAC properties */
		if (ctx->is_local) {
			zebra_evpn_mac_clear_fwd_info(mac);
			mac->fwd_info.local.ifindex = ctx->ifindex;
			mac->fwd_info.local.vid = ctx->vid;
			ZEBRA_MAC_SET_LOCAL(mac);
		} else {
			ZEBRA_MAC_SET_REMOTE(mac);
		}

		if (ctx->is_static)
			ZEBRA_MAC_SET_STATIC(mac);

		/* Associate with ES if ESI provided */
		if (ctx->has_esi) {
			/* This would integrate with EVPN MH */
			zlog_debug("notify: MAC %s associated with ESI %s",
				   mac_buf, esi_buf);
		}

	} else if (ctx->op == NOTIFY_OP_FDB_DELETE) {
		zlog_debug("notify: FDB_DELETE mac=%s vni=%u", mac_buf,
			   ctx->vni);

		mac = zebra_evpn_mac_lookup(zevpn, &ctx->mac);
		if (mac) {
			zebra_evpn_mac_del(zevpn, mac);
		}
	}

	return 0;
}

/* Process interface notification */
static int zebra_notify_process_intf(struct zebra_notify_ctx *ctx)
{
	struct interface *ifp;

	notify_intf_events++;

	if (ctx->op == NOTIFY_OP_INTF_ADD) {
		zlog_debug("notify: INTF_ADD ifname=%s ifindex=%u",
			   ctx->ifname, ctx->ifindex);

		/* This would create/update interface in zebra */
		ifp = if_lookup_by_index(ctx->ifindex, ctx->ns_id);
		if (!ifp) {
			ifp = if_create_name(ctx->ifname, ctx->ns_id);
			ifp->ifindex = ctx->ifindex;
		}

		ifp->flags = ctx->flags;
		ifp->mtu = ctx->mtu;
		memcpy(ifp->hw_addr, ctx->hw_addr, ETH_ALEN);

	} else if (ctx->op == NOTIFY_OP_INTF_UPDATE) {
		zlog_debug("notify: INTF_UPDATE ifindex=%u flags=0x%x",
			   ctx->ifindex, ctx->flags);

		ifp = if_lookup_by_index(ctx->ifindex, ctx->ns_id);
		if (ifp) {
			ifp->flags = ctx->flags;
		}
	} else if (ctx->op == NOTIFY_OP_INTF_DELETE) {
		zlog_debug("notify: INTF_DELETE ifindex=%u", ctx->ifindex);

		ifp = if_lookup_by_index(ctx->ifindex, ctx->ns_id);
		if (ifp) {
			if_delete(&ifp);
		}
	}

	return 0;
}

/* Process neighbor notification */
static int zebra_notify_process_neigh(struct zebra_notify_ctx *ctx)
{
	notify_neigh_events++;

	if (ctx->op == NOTIFY_OP_NEIGH_ADD) {
		char ip_buf[INET6_ADDRSTRLEN];
		char mac_buf[ETHER_ADDR_STRLEN];

		inet_ntop(ctx->family, &ctx->ip, ip_buf, sizeof(ip_buf));
		prefix_mac2str(&ctx->mac, mac_buf, sizeof(mac_buf));

		zlog_debug("notify: NEIGH_ADD ip=%s mac=%s ifindex=%u", ip_buf,
			   mac_buf, ctx->ifindex);

		/* This would update neighbor table in zebra */

	} else if (ctx->op == NOTIFY_OP_NEIGH_DELETE) {
		char ip_buf[INET6_ADDRSTRLEN];

		inet_ntop(ctx->family, &ctx->ip, ip_buf, sizeof(ip_buf));
		zlog_debug("notify: NEIGH_DELETE ip=%s", ip_buf);

		/* This would delete neighbor from zebra */
	}

	return 0;
}

/* Main injection point - providers call this to send events to zebra */
int zebra_notify_inject(struct zebra_notify_ctx *ctx)
{
	int ret = 0;

	if (!ctx) {
		zlog_err("notify: NULL context");
		return -1;
	}

	notify_total_events++;

	zlog_debug("notify: Processing %s event (total=%lu)",
		   zebra_notify_op2str(ctx->op), notify_total_events);

	/* Route to appropriate handler */
	switch (ctx->op) {
	case NOTIFY_OP_FDB_ADD:
	case NOTIFY_OP_FDB_DELETE:
		ret = zebra_notify_process_fdb(ctx);
		break;

	case NOTIFY_OP_INTF_ADD:
	case NOTIFY_OP_INTF_UPDATE:
	case NOTIFY_OP_INTF_DELETE:
		ret = zebra_notify_process_intf(ctx);
		break;

	case NOTIFY_OP_NEIGH_ADD:
	case NOTIFY_OP_NEIGH_DELETE:
		ret = zebra_notify_process_neigh(ctx);
		break;

	case NOTIFY_OP_VLAN_UPDATE:
		zlog_debug("notify: VLAN_UPDATE not yet implemented");
		break;

	default:
		zlog_warn("notify: Unknown operation %d", ctx->op);
		ret = -1;
		break;
	}

	/* Free context after processing */
	zebra_notify_ctx_free(&ctx);

	return ret;
}

/* Poll all registered providers */
void zebra_notify_poll_providers(void)
{
	uint32_t i;

	for (i = 0; i < notify_provider_count; i++) {
		struct zebra_notify_provider *prov = notify_providers[i];
		if (prov && prov->np_poll) {
			prov->np_poll(prov);
		}
	}
}

/* Timer for polling notification providers */
static struct event *notify_poll_timer;

/* Notification poll interval in milliseconds */
#define NOTIFY_POLL_INTERVAL_MS 100

/* Timer callback to poll notification providers */
static void zebra_notify_poll_timer(struct event *t)
{
	/* Poll all providers */
	zebra_notify_poll_providers();

	/* Reschedule */
	event_add_timer_msec(zrouter.master, zebra_notify_poll_timer, NULL,
			     NOTIFY_POLL_INTERVAL_MS, &notify_poll_timer);
}

/* Start notification polling */
void zebra_notify_start_poll(void)
{
	/* Schedule first poll */
	event_add_timer_msec(zrouter.master, zebra_notify_poll_timer, NULL,
			     NOTIFY_POLL_INTERVAL_MS, &notify_poll_timer);

	zlog_info("Zebra notification polling started (interval=%dms)",
		  NOTIFY_POLL_INTERVAL_MS);
}

/* Shutdown notification subsystem */
void zebra_notify_shutdown(void)
{
	uint32_t i;

	zlog_info(
		"Zebra notification subsystem shutdown (total_events=%lu, fdb=%lu, intf=%lu, neigh=%lu)",
		notify_total_events, notify_fdb_events, notify_intf_events,
		notify_neigh_events);

	for (i = 0; i < notify_provider_count; i++) {
		if (notify_providers[i]) {
			zebra_notify_provider_unregister(notify_providers[i]);
			notify_providers[i] = NULL;
		}
	}

	notify_provider_count = 0;
}
