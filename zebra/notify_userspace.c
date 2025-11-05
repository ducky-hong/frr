// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Userspace Notification Provider
 * Allows userspace programs to inject FDB/ARP/Interface events via Unix socket
 *
 * Copyright (C) 2025 FRR Community
 */

#include <zebra.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>
#include "lib/memory.h"
#include "lib/log.h"
#include "lib/json.h"
#include "zebra/zebra_notify.h"

DEFINE_MTYPE_STATIC(ZEBRA, NOTIFY_USERSPACE, "Userspace Notify Provider");

#define NOTIFY_SOCKET_PATH "/tmp/frr-notify.sock"
#define NOTIFY_MAX_MSG_SIZE 2048

/* Userspace provider private data */
struct userspace_notify {
	int listen_sock;
	int client_sock;
	char socket_path[256];
	char recv_buffer[NOTIFY_MAX_MSG_SIZE];
	size_t recv_offset;
};

/* Parse JSON message from userspace client */
static int parse_notify_message(const char *json_str,
				struct zebra_notify_ctx **ctx_out)
{
	json_object *jobj, *jval;
	struct zebra_notify_ctx *ctx;
	const char *op_str;
	const char *mac_str;
	const char *esi_str;

	ctx = zebra_notify_ctx_alloc();
	if (!ctx)
		return -1;

	jobj = json_tokener_parse(json_str);
	if (!jobj) {
		zlog_err("userspace_notify: Failed to parse JSON: %s",
			 json_str);
		zebra_notify_ctx_free(&ctx);
		return -1;
	}

	/* Parse operation */
	if (!json_object_object_get_ex(jobj, "op", &jval)) {
		zlog_err("userspace_notify: Missing 'op' field");
		json_object_put(jobj);
		zebra_notify_ctx_free(&ctx);
		return -1;
	}
	op_str = json_object_get_string(jval);

	if (strcmp(op_str, "fdb_add") == 0)
		ctx->op = NOTIFY_OP_FDB_ADD;
	else if (strcmp(op_str, "fdb_delete") == 0)
		ctx->op = NOTIFY_OP_FDB_DELETE;
	else if (strcmp(op_str, "intf_add") == 0)
		ctx->op = NOTIFY_OP_INTF_ADD;
	else if (strcmp(op_str, "intf_update") == 0)
		ctx->op = NOTIFY_OP_INTF_UPDATE;
	else if (strcmp(op_str, "neigh_add") == 0)
		ctx->op = NOTIFY_OP_NEIGH_ADD;
	else {
		zlog_err("userspace_notify: Unknown op '%s'", op_str);
		json_object_put(jobj);
		zebra_notify_ctx_free(&ctx);
		return -1;
	}

	/* Parse MAC address (for FDB events) */
	if (ctx->op == NOTIFY_OP_FDB_ADD || ctx->op == NOTIFY_OP_FDB_DELETE) {
		if (json_object_object_get_ex(jobj, "mac", &jval)) {
			mac_str = json_object_get_string(jval);
			if (!prefix_str2mac(mac_str, &ctx->mac)) {
				zlog_err(
					"userspace_notify: Invalid MAC address '%s'",
					mac_str);
				json_object_put(jobj);
				zebra_notify_ctx_free(&ctx);
				return -1;
			}
		}

		/* Parse VNI */
		if (json_object_object_get_ex(jobj, "vni", &jval))
			ctx->vni = json_object_get_int(jval);

		/* Parse VLAN ID */
		if (json_object_object_get_ex(jobj, "vid", &jval))
			ctx->vid = json_object_get_int(jval);

		/* Parse interface index */
		if (json_object_object_get_ex(jobj, "ifindex", &jval))
			ctx->ifindex = json_object_get_int(jval);

		/* Parse interface name */
		if (json_object_object_get_ex(jobj, "ifname", &jval)) {
			const char *ifname = json_object_get_string(jval);
			strlcpy(ctx->ifname, ifname, sizeof(ctx->ifname));
		}

		/* Parse ESI (for EVPN MH) */
		if (json_object_object_get_ex(jobj, "esi", &jval)) {
			esi_str = json_object_get_string(jval);
			if (str_to_esi(esi_str, &ctx->esi) == 0) {
				ctx->has_esi = true;
			} else {
				zlog_warn(
					"userspace_notify: Invalid ESI '%s'",
					esi_str);
			}
		}

		/* Parse local flag */
		if (json_object_object_get_ex(jobj, "local", &jval))
			ctx->is_local = json_object_get_boolean(jval);

		/* Parse static flag */
		if (json_object_object_get_ex(jobj, "static", &jval))
			ctx->is_static = json_object_get_boolean(jval);
	}

	/* Parse namespace ID */
	if (json_object_object_get_ex(jobj, "ns_id", &jval))
		ctx->ns_id = json_object_get_int(jval);
	else
		ctx->ns_id = NS_DEFAULT;

	json_object_put(jobj);
	*ctx_out = ctx;
	return 0;
}

/* Process received data from socket */
static int process_socket_data(struct userspace_notify *un)
{
	char *newline;
	struct zebra_notify_ctx *ctx;
	int ret;

	/* Look for complete message (newline-terminated) */
	newline = memchr(un->recv_buffer, '\n', un->recv_offset);
	if (!newline)
		return 0; /* Need more data */

	*newline = '\0';

	zlog_debug("userspace_notify: Received message: %s", un->recv_buffer);

	/* Parse and inject */
	ret = parse_notify_message(un->recv_buffer, &ctx);
	if (ret == 0) {
		zebra_notify_inject(ctx);
	}

	/* Remove processed message from buffer */
	size_t msg_len = newline - un->recv_buffer + 1;
	if (un->recv_offset > msg_len) {
		memmove(un->recv_buffer, newline + 1,
			un->recv_offset - msg_len);
		un->recv_offset -= msg_len;
	} else {
		un->recv_offset = 0;
	}

	return 1; /* Processed one message */
}

/* Poll callback - check for incoming data */
static int userspace_notify_poll(struct zebra_notify_provider *prov)
{
	struct userspace_notify *un = prov->np_data;
	struct sockaddr_un client_addr;
	socklen_t client_len;
	ssize_t nread;
	int processed = 0;

	if (!un)
		return -1;

	/* Accept new client if needed */
	if (un->client_sock < 0) {
		client_len = sizeof(client_addr);
		un->client_sock = accept(un->listen_sock,
					 (struct sockaddr *)&client_addr,
					 &client_len);
		if (un->client_sock >= 0) {
			zlog_info("userspace_notify: Client connected");
			/* Set non-blocking */
			fcntl(un->client_sock, F_SETFL,
			      fcntl(un->client_sock, F_GETFL, 0) | O_NONBLOCK);
		}
	}

	/* Read from client */
	if (un->client_sock >= 0) {
		nread = recv(un->client_sock,
			     un->recv_buffer + un->recv_offset,
			     sizeof(un->recv_buffer) - un->recv_offset - 1, 0);

		if (nread > 0) {
			un->recv_offset += nread;
			un->recv_buffer[un->recv_offset] = '\0';

			/* Process all complete messages */
			while (process_socket_data(un) > 0)
				processed++;

		} else if (nread == 0 || (nread < 0 && errno != EAGAIN)) {
			/* Client disconnected */
			zlog_info("userspace_notify: Client disconnected");
			close(un->client_sock);
			un->client_sock = -1;
			un->recv_offset = 0;
		}
	}

	return processed;
}

/* Start callback - create socket */
static int userspace_notify_start(struct zebra_notify_provider *prov)
{
	struct userspace_notify *un = prov->np_data;
	struct sockaddr_un addr;
	int ret;

	if (!un)
		return -1;

	/* Create Unix domain socket */
	un->listen_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (un->listen_sock < 0) {
		zlog_err("userspace_notify: Failed to create socket: %s",
			 strerror(errno));
		return -1;
	}

	/* Set non-blocking */
	fcntl(un->listen_sock, F_SETFL,
	      fcntl(un->listen_sock, F_GETFL, 0) | O_NONBLOCK);

	/* Remove old socket file if exists */
	unlink(un->socket_path);

	/* Bind */
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strlcpy(addr.sun_path, un->socket_path, sizeof(addr.sun_path));

	ret = bind(un->listen_sock, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0) {
		zlog_err("userspace_notify: Failed to bind to %s: %s",
			 un->socket_path, strerror(errno));
		close(un->listen_sock);
		return -1;
	}

	/* Listen */
	ret = listen(un->listen_sock, 5);
	if (ret < 0) {
		zlog_err("userspace_notify: Failed to listen: %s",
			 strerror(errno));
		close(un->listen_sock);
		unlink(un->socket_path);
		return -1;
	}

	zlog_info("userspace_notify: Listening on %s", un->socket_path);
	return 0;
}

/* Stop callback - cleanup */
static int userspace_notify_stop(struct zebra_notify_provider *prov)
{
	struct userspace_notify *un = prov->np_data;

	if (!un)
		return -1;

	if (un->client_sock >= 0) {
		close(un->client_sock);
		un->client_sock = -1;
	}

	if (un->listen_sock >= 0) {
		close(un->listen_sock);
		unlink(un->socket_path);
		un->listen_sock = -1;
	}

	zlog_info("userspace_notify: Stopped");
	return 0;
}

/* Initialize userspace notification provider */
int notify_userspace_init(const char *socket_path)
{
	struct userspace_notify *un;
	struct zebra_notify_provider *prov;
	int ret;

	un = XCALLOC(MTYPE_NOTIFY_USERSPACE, sizeof(struct userspace_notify));

	un->listen_sock = -1;
	un->client_sock = -1;
	un->recv_offset = 0;

	if (socket_path)
		strlcpy(un->socket_path, socket_path, sizeof(un->socket_path));
	else
		strlcpy(un->socket_path, NOTIFY_SOCKET_PATH,
			sizeof(un->socket_path));

	ret = zebra_notify_provider_register(
		"userspace", userspace_notify_start, userspace_notify_poll,
		userspace_notify_stop, un, &prov);

	if (ret < 0) {
		XFREE(MTYPE_NOTIFY_USERSPACE, un);
		return -1;
	}

	zlog_info("Userspace notification provider initialized");
	return 0;
}

/* Cleanup */
void notify_userspace_cleanup(void)
{
	/* Provider will be cleaned up by zebra_notify_shutdown() */
}
