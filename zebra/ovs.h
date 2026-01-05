// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Open vSwitch (OVS) backend hooks for zebra.
 */

#ifndef _ZEBRA_OVS_H
#define _ZEBRA_OVS_H

#include <stdbool.h>

#include "lib/zebra.h"

#ifdef __cplusplus
extern "C" {
#endif

struct zebra_ns;
struct zebra_dplane_ctx;
struct dplane_ctx_list_head;

bool zebra_ovs_is_enabled(void);
void zebra_ovs_set_enabled(bool enabled);
void zebra_ovs_set_fdb_bridge(const char *bridge);
void zebra_ovs_set_arp_bridge(const char *bridge);
void zebra_ovs_set_ofctl_path(const char *path);
void zebra_ovs_set_poll_interval(uint32_t seconds);

void zebra_ovs_init(struct zebra_ns *zns);
void zebra_ovs_terminate(struct zebra_ns *zns);

void zebra_ovs_interface_list(struct zebra_ns *zns);
void zebra_ovs_interface_list_tunneldump(struct zebra_ns *zns);
void zebra_ovs_interface_list_second(struct zebra_ns *zns);

void zebra_ovs_update_multi(struct dplane_ctx_list_head *ctx_list);
int zebra_ovs_dplane_register(void);

#ifdef __cplusplus
}
#endif

#endif /* _ZEBRA_OVS_H */
