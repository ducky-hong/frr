// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Test program for zebra notification provider
 * Demonstrates userspace injection of FDB events for EVPN MH
 *
 * Copyright (C) 2025 FRR Community
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#define SOCKET_PATH_DEFAULT "/tmp/frr-notify.sock"

/* Send a JSON message to the zebra notification socket */
static int send_notify(const char *json_msg)
{
	int sock;
	struct sockaddr_un addr;
	int ret;
	size_t len;
	const char *socket_path;

	/* Get socket path from environment or use default */
	socket_path = getenv("SOCKET_PATH");
	if (!socket_path)
		socket_path = SOCKET_PATH_DEFAULT;

	/* Create socket */
	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		fprintf(stderr, "Failed to create socket: %s\n",
			strerror(errno));
		return -1;
	}

	/* Connect to zebra */
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

	ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0) {
		fprintf(stderr, "Failed to connect to %s: %s\n", socket_path,
			strerror(errno));
		fprintf(stderr,
			"Make sure zebra is running with userspace notification provider enabled\n");
		close(sock);
		return -1;
	}

	printf("Connected to zebra notification socket\n");

	/* Send message (must be newline-terminated) */
	len = strlen(json_msg);
	ret = send(sock, json_msg, len, 0);
	if (ret < 0) {
		fprintf(stderr, "Failed to send message: %s\n",
			strerror(errno));
		close(sock);
		return -1;
	}

	/* Send newline */
	send(sock, "\n", 1, 0);

	printf("Sent notification: %s\n", json_msg);

	close(sock);
	return 0;
}

/* Test scenario 1: Simple FDB learn event without ESI */
static void test_simple_fdb(void)
{
	const char *msg;

	printf("\n=== Test 1: Simple FDB learn (no ESI) ===\n");

	msg = "{\"op\":\"fdb_add\",\"mac\":\"00:11:22:33:44:55\",\"vni\":1000,\"ifindex\":10,\"ifname\":\"vxlan1000\",\"vid\":1000,\"local\":true,\"static\":false}";
	send_notify(msg);

	sleep(1);
}

/* Test scenario 2: EVPN MH FDB learn with ESI */
static void test_evpn_mh_fdb(void)
{
	const char *msg;

	printf("\n=== Test 2: EVPN MH FDB learn (with ESI) ===\n");

	/* Simulate MAC learned on ES bond - this is the EVPN MH case */
	msg = "{\"op\":\"fdb_add\",\"mac\":\"00:00:00:00:00:11\",\"vni\":1000,\"ifindex\":20,\"ifname\":\"hostbond1\",\"vid\":1000,\"local\":true,\"static\":false,\"esi\":\"03:44:38:39:ff:ff:01:00:00:01\"}";
	send_notify(msg);

	sleep(1);

	/* Another MAC on same ES */
	msg = "{\"op\":\"fdb_add\",\"mac\":\"00:00:00:00:00:12\",\"vni\":1000,\"ifindex\":20,\"ifname\":\"hostbond1\",\"vid\":1000,\"local\":true,\"static\":false,\"esi\":\"03:44:38:39:ff:ff:01:00:00:01\"}";
	send_notify(msg);

	sleep(1);
}

/* Test scenario 3: Remote MAC (from peer PE) */
static void test_remote_mac(void)
{
	const char *msg;

	printf("\n=== Test 3: Remote MAC (from peer VTEP) ===\n");

	msg = "{\"op\":\"fdb_add\",\"mac\":\"00:00:00:00:00:21\",\"vni\":1000,\"ifindex\":10,\"ifname\":\"vxlan1000\",\"vid\":1000,\"local\":false,\"static\":false,\"esi\":\"03:44:38:39:ff:ff:02:00:00:01\"}";
	send_notify(msg);

	sleep(1);
}

/* Test scenario 4: MAC aging/delete */
static void test_fdb_delete(void)
{
	const char *msg;

	printf("\n=== Test 4: MAC aging/delete ===\n");

	msg = "{\"op\":\"fdb_delete\",\"mac\":\"00:11:22:33:44:55\",\"vni\":1000}";
	send_notify(msg);

	sleep(1);
}

/* Test scenario 5: Multiple MACs for EVPN MH peer sync */
static void test_evpn_mh_peer_sync(void)
{
	const char *msgs[] = {
		/* MACs learned locally on ES 01:00:00:01 */
		"{\"op\":\"fdb_add\",\"mac\":\"aa:bb:cc:00:00:01\",\"vni\":1000,\"ifindex\":20,\"ifname\":\"hostbond1\",\"vid\":1000,\"local\":true,\"esi\":\"03:44:38:39:ff:ff:01:00:00:01\"}",
		"{\"op\":\"fdb_add\",\"mac\":\"aa:bb:cc:00:00:02\",\"vni\":1000,\"ifindex\":20,\"ifname\":\"hostbond1\",\"vid\":1000,\"local\":true,\"esi\":\"03:44:38:39:ff:ff:01:00:00:01\"}",
		"{\"op\":\"fdb_add\",\"mac\":\"aa:bb:cc:00:00:03\",\"vni\":1000,\"ifindex\":20,\"ifname\":\"hostbond1\",\"vid\":1000,\"local\":true,\"esi\":\"03:44:38:39:ff:ff:01:00:00:01\"}",

		/* MACs from peer PE on same ES (synced) */
		"{\"op\":\"fdb_add\",\"mac\":\"aa:bb:cc:00:00:01\",\"vni\":1000,\"ifindex\":10,\"ifname\":\"vxlan1000\",\"vid\":1000,\"local\":false,\"esi\":\"03:44:38:39:ff:ff:01:00:00:01\"}",
		"{\"op\":\"fdb_add\",\"mac\":\"aa:bb:cc:00:00:02\",\"vni\":1000,\"ifindex\":10,\"ifname\":\"vxlan1000\",\"vid\":1000,\"local\":false,\"esi\":\"03:44:38:39:ff:ff:01:00:00:01\"}",

		/* MACs from different ES */
		"{\"op\":\"fdb_add\",\"mac\":\"dd:ee:ff:00:00:01\",\"vni\":1000,\"ifindex\":10,\"ifname\":\"vxlan1000\",\"vid\":1000,\"local\":false,\"esi\":\"03:44:38:39:ff:ff:02:00:00:01\"}",
	};

	printf(
		"\n=== Test 5: EVPN MH Peer Sync (local + remote MACs on same ES) ===\n");

	for (size_t i = 0; i < sizeof(msgs) / sizeof(msgs[0]); i++) {
		send_notify(msgs[i]);
		usleep(100000); /* 100ms between messages */
	}

	sleep(1);
}

static void print_usage(const char *prog)
{
	printf("Usage: %s [test_number]\n", prog);
	printf("  test_number: 1-5 (default: run all tests)\n");
	printf("    1: Simple FDB learn\n");
	printf("    2: EVPN MH FDB learn with ESI\n");
	printf("    3: Remote MAC from peer VTEP\n");
	printf("    4: MAC aging/delete\n");
	printf("    5: EVPN MH peer sync scenario\n");
	printf("\n");
	printf("Example:\n");
	printf("  %s       # Run all tests\n", prog);
	printf("  %s 2     # Run only EVPN MH test\n", prog);
	printf("  %s 5     # Run only peer sync test\n", prog);
}

int main(int argc, char **argv)
{
	int test_num = 0;

	if (argc > 1) {
		if (strcmp(argv[1], "-h") == 0 ||
		    strcmp(argv[1], "--help") == 0) {
			print_usage(argv[0]);
			return 0;
		}
		test_num = atoi(argv[1]);
		if (test_num < 1 || test_num > 5) {
			fprintf(stderr, "Invalid test number: %d\n", test_num);
			print_usage(argv[0]);
			return 1;
		}
	}

	printf("Zebra Notification Provider Test\n");
	printf("=================================\n");

	if (test_num == 0 || test_num == 1)
		test_simple_fdb();

	if (test_num == 0 || test_num == 2)
		test_evpn_mh_fdb();

	if (test_num == 0 || test_num == 3)
		test_remote_mac();

	if (test_num == 0 || test_num == 4)
		test_fdb_delete();

	if (test_num == 0 || test_num == 5)
		test_evpn_mh_peer_sync();

	printf("\n=== All tests completed ===\n");
	printf(
		"Check zebra logs for notification processing (debug zebra evpn)\n");

	return 0;
}
