// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Zebra userspace mock provider - for testing without kernel
 * Copyright (C) 2025 Free Range Routing
 *
 * This mock provider allows injecting events through a Unix socket,
 * enabling testing of EVPN multihoming and other features without
 * requiring kernel dependencies.
 */

#include "zebra.h"

#include <sys/un.h>
#include <sys/socket.h>

#include "log.h"
#include "memory.h"
#include "prefix.h"
#include "command.h"
#include "vty.h"
#include "lib_errors.h"
#include "zebra/zebra_router.h"
#include "zebra/zebra_errors.h"
#include "zebra/zebra_usrspace_provider.h"

#define MOCK_SOCKET_PATH "/var/run/frr/zebra_usrspace.sock"
#define MOCK_QUEUE_SIZE 1024

DEFINE_MTYPE_STATIC(ZEBRA, USRSPACE_MOCK, "Userspace Mock Provider");

/* Mock provider private data */
struct usrspace_mock_ctx {
	int sock;  /* Unix domain socket for receiving commands */
	char sock_path[256];

	/* Event queue */
	struct usrspace_event queue[MOCK_QUEUE_SIZE];
	int queue_head;
	int queue_tail;
	int queue_count;

	/* Statistics */
	uint64_t events_received;
	uint64_t events_processed;
	uint64_t events_dropped;
};

/* Wire protocol for events over Unix socket */
struct usrspace_wire_event {
	uint32_t magic;        /* Magic number for validation */
	uint32_t version;      /* Protocol version */
	uint32_t type;         /* Event type */
	uint32_t ns_id;        /* Namespace ID */
	uint8_t data[4096];    /* Event-specific data */
} __attribute__((packed));

#define USRSPACE_MAGIC 0x55535250  /* "USRP" */
#define USRSPACE_VERSION 1

/*
 * Initialize mock provider
 */
static int usrspace_mock_init(struct usrspace_provider *prov)
{
	struct usrspace_mock_ctx *ctx;

	ctx = XCALLOC(MTYPE_USRSPACE_MOCK, sizeof(*ctx));
	ctx->sock = -1;
	snprintf(ctx->sock_path, sizeof(ctx->sock_path), "%s",
		 MOCK_SOCKET_PATH);
	ctx->queue_head = 0;
	ctx->queue_tail = 0;
	ctx->queue_count = 0;

	prov->priv = ctx;

	if (IS_ZEBRA_DEBUG_EVENT)
		zlog_debug("%s: Mock provider initialized", __func__);

	return 0;
}

/*
 * Shutdown mock provider
 */
static int usrspace_mock_fini(struct usrspace_provider *prov)
{
	struct usrspace_mock_ctx *ctx = prov->priv;

	if (!ctx)
		return 0;

	if (ctx->sock >= 0) {
		close(ctx->sock);
		unlink(ctx->sock_path);
	}

	XFREE(MTYPE_USRSPACE_MOCK, ctx);
	prov->priv = NULL;

	if (IS_ZEBRA_DEBUG_EVENT)
		zlog_debug("%s: Mock provider shut down", __func__);

	return 0;
}

/*
 * Start listening for events
 */
static int usrspace_mock_start(struct usrspace_provider *prov)
{
	struct usrspace_mock_ctx *ctx = prov->priv;
	struct sockaddr_un addr;
	int ret;

	if (!ctx)
		return -1;

	/* Create Unix domain socket */
	ctx->sock = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (ctx->sock < 0) {
		flog_err_sys(EC_LIB_SOCKET,
			     "%s: Failed to create socket: %s", __func__,
			     safe_strerror(errno));
		return -1;
	}

	/* Remove old socket file if it exists */
	unlink(ctx->sock_path);

	/* Bind to socket path */
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strlcpy(addr.sun_path, ctx->sock_path, sizeof(addr.sun_path));

	ret = bind(ctx->sock, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0) {
		flog_err_sys(EC_LIB_SOCKET, "%s: Failed to bind socket: %s",
			     __func__, safe_strerror(errno));
		close(ctx->sock);
		ctx->sock = -1;
		return -1;
	}

	/* Set permissions so tests can write to it */
	chmod(ctx->sock_path, 0666);

	if (IS_ZEBRA_DEBUG_EVENT)
		zlog_debug("%s: Mock provider listening on %s", __func__,
			   ctx->sock_path);

	return 0;
}

/*
 * Stop listening for events
 */
static int usrspace_mock_stop(struct usrspace_provider *prov)
{
	struct usrspace_mock_ctx *ctx = prov->priv;

	if (!ctx)
		return 0;

	if (ctx->sock >= 0) {
		close(ctx->sock);
		unlink(ctx->sock_path);
		ctx->sock = -1;
	}

	if (IS_ZEBRA_DEBUG_EVENT)
		zlog_debug("%s: Mock provider stopped", __func__);

	return 0;
}

/*
 * Get file descriptor for event loop
 */
static int usrspace_mock_get_fd(struct usrspace_provider *prov)
{
	struct usrspace_mock_ctx *ctx = prov->priv;

	if (!ctx)
		return -1;

	return ctx->sock;
}

/*
 * Decode wire event to internal format
 */
static int usrspace_mock_decode_event(struct usrspace_wire_event *wire,
				      struct usrspace_event *event)
{
	if (wire->magic != USRSPACE_MAGIC) {
		zlog_err("%s: Invalid magic number: 0x%08x", __func__,
			 wire->magic);
		return -1;
	}

	if (wire->version != USRSPACE_VERSION) {
		zlog_err("%s: Unsupported version: %u", __func__,
			 wire->version);
		return -1;
	}

	memset(event, 0, sizeof(*event));
	event->type = wire->type;
	event->ns_id = wire->ns_id;

	/* Decode event-specific data */
	switch (event->type) {
	case USRSPACE_EVENT_INTF_ADD:
	case USRSPACE_EVENT_INTF_DELETE:
	case USRSPACE_EVENT_INTF_UP:
	case USRSPACE_EVENT_INTF_DOWN:
		memcpy(&event->u.intf, wire->data,
		       sizeof(struct usrspace_intf_event));
		break;

	case USRSPACE_EVENT_INTF_ADDR_ADD:
	case USRSPACE_EVENT_INTF_ADDR_DEL:
		memcpy(&event->u.addr, wire->data,
		       sizeof(struct usrspace_addr_event));
		break;

	case USRSPACE_EVENT_MAC_ADD:
	case USRSPACE_EVENT_MAC_DEL:
		memcpy(&event->u.mac, wire->data,
		       sizeof(struct usrspace_mac_event));
		break;

	case USRSPACE_EVENT_NEIGH_ADD:
	case USRSPACE_EVENT_NEIGH_UPDATE:
	case USRSPACE_EVENT_NEIGH_DEL:
		memcpy(&event->u.neigh, wire->data,
		       sizeof(struct usrspace_neigh_event));
		break;

	case USRSPACE_EVENT_BR_PORT_UPDATE:
		memcpy(&event->u.br_port, wire->data,
		       sizeof(struct usrspace_br_port_event));
		break;

	case USRSPACE_EVENT_VLAN_ADD:
	case USRSPACE_EVENT_VLAN_DEL:
		memcpy(&event->u.vlan, wire->data,
		       sizeof(struct usrspace_vlan_event));
		break;

	default:
		zlog_err("%s: Unknown event type: %u", __func__, wire->type);
		return -1;
	}

	return 0;
}

/*
 * Add event to queue
 */
static int usrspace_mock_enqueue(struct usrspace_mock_ctx *ctx,
				 struct usrspace_event *event)
{
	if (ctx->queue_count >= MOCK_QUEUE_SIZE) {
		ctx->events_dropped++;
		zlog_warn("%s: Event queue full, dropping event", __func__);
		return -1;
	}

	ctx->queue[ctx->queue_tail] = *event;
	ctx->queue_tail = (ctx->queue_tail + 1) % MOCK_QUEUE_SIZE;
	ctx->queue_count++;

	return 0;
}

/*
 * Get event from queue
 */
static int usrspace_mock_dequeue(struct usrspace_mock_ctx *ctx,
				 struct usrspace_event *event)
{
	if (ctx->queue_count == 0)
		return -1;

	*event = ctx->queue[ctx->queue_head];
	ctx->queue_head = (ctx->queue_head + 1) % MOCK_QUEUE_SIZE;
	ctx->queue_count--;

	return 0;
}

/*
 * Poll for events
 */
static int usrspace_mock_poll(struct usrspace_provider *prov)
{
	struct usrspace_mock_ctx *ctx = prov->priv;
	struct usrspace_wire_event wire;
	struct usrspace_event event;
	ssize_t len;
	int processed = 0;

	if (!ctx || ctx->sock < 0)
		return -1;

	/* Read events from socket and queue them */
	while (1) {
		len = recv(ctx->sock, &wire, sizeof(wire), MSG_DONTWAIT);
		if (len < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;

			zlog_err("%s: recv failed: %s", __func__,
				 safe_strerror(errno));
			break;
		}

		if (len != sizeof(wire)) {
			zlog_err("%s: Received incomplete message: %zd bytes",
				 __func__, len);
			continue;
		}

		ctx->events_received++;

		/* Decode and queue event */
		if (usrspace_mock_decode_event(&wire, &event) < 0) {
			zlog_err("%s: Failed to decode event", __func__);
			ctx->events_dropped++;
			continue;
		}

		if (usrspace_mock_enqueue(ctx, &event) < 0) {
			zlog_err("%s: Failed to enqueue event", __func__);
			continue;
		}
	}

	/* Process queued events */
	while (usrspace_mock_dequeue(ctx, &event) == 0) {
		if (zebra_usrspace_inject_event(&event) == 0) {
			ctx->events_processed++;
			processed++;
		} else {
			zlog_err("%s: Failed to inject event", __func__);
		}
	}

	return processed;
}

/* Provider operations */
static const struct usrspace_provider_ops mock_ops = {
	.init = usrspace_mock_init,
	.fini = usrspace_mock_fini,
	.start = usrspace_mock_start,
	.stop = usrspace_mock_stop,
	.get_fd = usrspace_mock_get_fd,
	.poll = usrspace_mock_poll,
};

/* Provider instance */
static struct usrspace_provider mock_provider = {
	.name = "mock",
	.ops = &mock_ops,
	.priv = NULL,
	.enabled = false,
	.started = false,
};

/*
 * Register mock provider
 */
void zebra_usrspace_mock_init(void)
{
	zebra_usrspace_provider_register(&mock_provider);

	if (IS_ZEBRA_DEBUG_EVENT)
		zlog_debug("%s: Mock provider registered", __func__);
}

/*
 * Direct event injection API (for testing)
 */
int zebra_usrspace_mock_inject(struct usrspace_event *event)
{
	if (!zebra_usrspace_is_enabled()) {
		zlog_warn("%s: Userspace mode not enabled", __func__);
		return -1;
	}

	return zebra_usrspace_inject_event(event);
}

/*
 * CLI commands
 */

DEFUN(show_userspace_provider,
      show_userspace_provider_cmd,
      "show userspace-dataplane provider",
      SHOW_STR
      "Userspace dataplane\n"
      "Provider information\n")
{
	struct usrspace_provider *prov = zebra_usrspace_get_provider();
	struct usrspace_mock_ctx *ctx;

	if (!prov) {
		vty_out(vty, "No userspace provider registered\n");
		return CMD_SUCCESS;
	}

	vty_out(vty, "Provider: %s\n", prov->name);
	vty_out(vty, "Enabled: %s\n", prov->enabled ? "yes" : "no");
	vty_out(vty, "Started: %s\n", prov->started ? "yes" : "no");

	if (strcmp(prov->name, "mock") == 0 && prov->priv) {
		ctx = prov->priv;
		vty_out(vty, "\nMock Provider Statistics:\n");
		vty_out(vty, "  Socket path: %s\n", ctx->sock_path);
		vty_out(vty, "  Events received: %" PRIu64 "\n",
			ctx->events_received);
		vty_out(vty, "  Events processed: %" PRIu64 "\n",
			ctx->events_processed);
		vty_out(vty, "  Events dropped: %" PRIu64 "\n",
			ctx->events_dropped);
		vty_out(vty, "  Queue size: %d\n", ctx->queue_count);
	}

	return CMD_SUCCESS;
}

DEFUN(userspace_dataplane,
      userspace_dataplane_cmd,
      "userspace-dataplane",
      "Enable userspace dataplane mode\n")
{
	zebra_usrspace_set_enabled(true);
	return CMD_SUCCESS;
}

DEFUN(no_userspace_dataplane,
      no_userspace_dataplane_cmd,
      "no userspace-dataplane",
      NO_STR
      "Disable userspace dataplane mode\n")
{
	zebra_usrspace_set_enabled(false);
	return CMD_SUCCESS;
}

void zebra_usrspace_mock_vty_init(void)
{
	install_element(VIEW_NODE, &show_userspace_provider_cmd);
	install_element(CONFIG_NODE, &userspace_dataplane_cmd);
	install_element(CONFIG_NODE, &no_userspace_dataplane_cmd);
}
