// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Open vSwitch (OVS) flow parsing helpers.
 */

#ifndef _ZEBRA_OVS_PARSE_H
#define _ZEBRA_OVS_PARSE_H

#include <stdbool.h>
#include <netinet/in.h>

#include "lib/prefix.h"
#include "lib/vlan.h"
#include "lib/vxlan.h"

#ifdef __cplusplus
extern "C" {
#endif

bool zebra_ovs_parse_token(const char *line, const char *key,
			   char *out, size_t outlen);
bool zebra_ovs_parse_event_is_del(const char *line);
bool zebra_ovs_parse_fdb_line(const char *line, struct ethaddr *mac,
			      uint32_t *ofport);
bool zebra_ovs_parse_arp_line(const char *line, struct ipaddr *ip,
			      struct ethaddr *mac, uint32_t *ofport);
bool zebra_ovs_parse_vni(const char *value, vni_t *vni);
bool zebra_ovs_parse_vlan(const char *value, vlanid_t *vid);
bool zebra_ovs_parse_ipv4(const char *value, struct in_addr *addr);

#ifdef __cplusplus
}
#endif

#endif /* _ZEBRA_OVS_PARSE_H */
