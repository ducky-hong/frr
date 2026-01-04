// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * OVS flow parsing helper tests.
 */

#include <zebra.h>

#include <string.h>

#include "lib/ipaddr.h"
#include "lib/module.h"
#include "lib/prefix.h"
#include "zebra/ovs_parse.h"

static const struct frrmod_info ovs_parse_test_info = {
	.name = "test_ovs_parse",
	.version = "0",
	.description = "OVS parsing helpers test",
};

union _frrmod_runtime_u _frrmod_this_module
	__attribute__((visibility("default"))) = {
		.r =
			{
				.info = &ovs_parse_test_info,
				.finished_loading = 1,
			},
};

static void print_token(const char *label, const char *line, const char *key)
{
	char out[64];
	bool ok;

	memset(out, 0, sizeof(out));
	ok = zebra_ovs_parse_token(line, key, out, sizeof(out));
	printf("token %s ok=%d value=%s\n", label, ok ? 1 : 0,
	       ok ? out : "-");
}

static void print_event(const char *label, const char *line)
{
	bool ok = zebra_ovs_parse_event_is_del(line);

	printf("event %s del=%d\n", label, ok ? 1 : 0);
}

static void print_fdb(const char *label, const char *line)
{
	struct ethaddr mac;
	uint32_t port = 0;
	char macbuf[32];
	bool ok;
	const char *mac_str = "-";

	ok = zebra_ovs_parse_fdb_line(line, &mac, &port);
	if (ok)
		mac_str = prefix_mac2str(&mac, macbuf, sizeof(macbuf));

	printf("fdb %s ok=%d mac=%s port=%u\n", label, ok ? 1 : 0,
	       mac_str, ok ? port : 0);
}

static void print_arp(const char *label, const char *line)
{
	struct ipaddr ip;
	struct ethaddr mac;
	uint32_t port = 0;
	char ipbuf[IPADDR_STRING_SIZE];
	char macbuf[32];
	bool ok;
	const char *ip_str = "-";
	const char *mac_str = "-";

	ok = zebra_ovs_parse_arp_line(line, &ip, &mac, &port);
	if (ok) {
		ip_str = ipaddr2str(&ip, ipbuf, sizeof(ipbuf));
		mac_str = prefix_mac2str(&mac, macbuf, sizeof(macbuf));
	}

	printf("arp %s ok=%d ip=%s mac=%s port=%u\n", label, ok ? 1 : 0,
	       ip_str, mac_str, ok ? port : 0);
}

int main(int argc, char **argv)
{
	const char *token_line =
		"cookie=0x0, duration=0.5, in_port=5,dl_dst=aa:bb:cc:dd:ee:ff actions=drop";
	const char *fdb_line =
		"in_port=10, dl_dst=00:11:22:33:44:55, actions=output:1";
	const char *fdb_missing =
		"dl_dst=00:11:22:33:44:55, actions=output:1";
	const char *arp_v4 =
		"arp_tpa=192.0.2.1, arp_sha=aa:bb:cc:dd:ee:ff, in_port=7";
	const char *arp_v6 =
		"arp_tpa=2001:db8::1, dl_src=01:02:03:04:05:06, in_port=3";
	const char *arp_missing =
		"arp_tpa=192.0.2.2, arp_sha=aa:bb:cc:dd:ee:ff";

	print_token("in_port", token_line, "in_port");
	print_token("dl_dst", token_line, "dl_dst");
	print_token("actions", token_line, "actions");
	print_token("missing", token_line, "missing");

	print_event("DEL", "event=DEL, in_port=5");
	print_event("delete", "event=delete, in_port=5");
	print_event("ADD", "event=ADD, in_port=5");
	print_event("none", "in_port=5");

	print_fdb("ok", fdb_line);
	print_fdb("missing", fdb_missing);

	print_arp("v4", arp_v4);
	print_arp("v6", arp_v6);
	print_arp("missing-port", arp_missing);

	(void)argc;
	(void)argv;
	return 0;
}
