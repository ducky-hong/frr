// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Open vSwitch (OVS) flow parsing helpers.
 */

#include "zebra.h"

#include <string.h>

#include "zebra/ovs_parse.h"

bool zebra_ovs_parse_token(const char *line, const char *key,
			   char *out, size_t outlen)
{
	const char *p;
	size_t len;

	if (!line || !key || !out || outlen == 0)
		return false;

	p = strstr(line, key);
	if (!p)
		return false;
	p += strlen(key);
	if (*p != '=')
		return false;
	p++;

	len = strcspn(p, ", \t\r\n");
	if (len == 0 || len >= outlen)
		return false;
	memcpy(out, p, len);
	out[len] = '\0';
	return true;
}

bool zebra_ovs_parse_event_is_del(const char *line)
{
	const char *p = strstr(line, "event=");

	if (!p)
		return false;
	if (strstr(p, "DEL") || strstr(p, "delete") || strstr(p, "DELETE"))
		return true;
	return false;
}

bool zebra_ovs_parse_fdb_line(const char *line, struct ethaddr *mac,
			      uint32_t *ofport)
{
	char macbuf[64];
	char portbuf[32];

	if (!mac || !ofport)
		return false;
	if (!zebra_ovs_parse_token(line, "dl_dst", macbuf, sizeof(macbuf)))
		return false;
	if (!zebra_ovs_parse_token(line, "in_port", portbuf, sizeof(portbuf)))
		return false;
	if (!prefix_str2mac(macbuf, mac))
		return false;

	*ofport = (uint32_t)strtoul(portbuf, NULL, 10);
	return true;
}

bool zebra_ovs_parse_arp_line(const char *line, struct ipaddr *ip,
			      struct ethaddr *mac, uint32_t *ofport)
{
	char ipbuf[64];
	char macbuf[64];
	char portbuf[32];

	if (!ip || !mac || !ofport)
		return false;
	if (!zebra_ovs_parse_token(line, "arp_tpa", ipbuf, sizeof(ipbuf)))
		return false;
	if (!zebra_ovs_parse_token(line, "arp_sha", macbuf, sizeof(macbuf)) &&
	    !zebra_ovs_parse_token(line, "dl_src", macbuf, sizeof(macbuf)))
		return false;
	if (!zebra_ovs_parse_token(line, "in_port", portbuf, sizeof(portbuf)))
		return false;
	if (str2ipaddr(ipbuf, ip) != 0)
		return false;
	if (!prefix_str2mac(macbuf, mac))
		return false;

	*ofport = (uint32_t)strtoul(portbuf, NULL, 10);
	return true;
}
