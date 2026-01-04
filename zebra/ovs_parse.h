// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Open vSwitch (OVS) flow parsing helpers.
 */

#ifndef _ZEBRA_OVS_PARSE_H
#define _ZEBRA_OVS_PARSE_H

#include <stdbool.h>

#include "lib/prefix.h"

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

#ifdef __cplusplus
}
#endif

#endif /* _ZEBRA_OVS_PARSE_H */
