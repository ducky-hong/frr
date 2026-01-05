// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Open vSwitch (OVS) backend for zebra (EVPN-MH focused).
 */

#include "zebra.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "lib/hash.h"
#include "lib/json.h"
#include "lib/memory.h"
#include "lib/prefix.h"
#include "printfrr.h"
#include "lib/if.h"

#include "zebra/interface.h"
#include "zebra/ovs.h"
#include "zebra/ovs_parse.h"
#include "zebra/zapi_msg.h"
#include "zebra/zebra_dplane.h"
#include "zebra/zebra_evpn_mh.h"
#include "zebra/zebra_l2.h"
#include "zebra/zebra_neigh.h"
#include "zebra/zebra_router.h"
#include "zebra/zebra_vxlan.h"

#ifndef NUD_REACHABLE
#define NUD_REACHABLE 0
#endif

DEFINE_MTYPE_STATIC(ZEBRA, OVS, "Zebra OVS");
DEFINE_MTYPE_STATIC(ZEBRA, OVS_BUF, "Zebra OVS Buffer");
DEFINE_MTYPE_STATIC(ZEBRA, OVS_KEY, "Zebra OVS Key");

enum ovs_monitor_type {
	OVS_MON_FDB,
	OVS_MON_ARP,
};

struct ovs_config {
	bool enabled;
	char *fdb_bridge;
	char *arp_bridge;
	char *ofctl_path;
	uint32_t poll_interval;
};

struct ovs_monitor {
	enum ovs_monitor_type type;
	char *bridge;
	pid_t pid;
	int fd;
	struct event *t_read;
	char buf[8192];
	size_t len;
};

struct ovs_ofport_map {
	uint32_t ofport;
	struct interface *ifp;
};

struct ovs_key_entry {
	char *key;
};

struct ovs_fdb_entry {
	char *key;
	struct ethaddr mac;
	uint32_t ofport;
};

struct ovs_arp_entry {
	char *key;
	struct ethaddr mac;
	struct ipaddr ip;
	uint32_t ofport;
};

struct ovs_ifinfo {
	char *name;
	char *bridge_name;
	char *mac;
	char *bond_master;
	char *esi;
	int64_t ofport;
	int64_t ifindex;
	int64_t mtu;
	bool admin_up;
	bool link_up;
	bool is_bridge;
	bool is_vxlan;
	bool is_bond;
	bool has_vni;
	bool has_vtep_ip;
	bool has_mcast_grp;
	bool has_access_vlan;
	vni_t vni;
	vlanid_t access_vlan;
	struct in_addr vtep_ip;
	struct in_addr mcast_grp;
};

static struct ovs_config ovs_cfg = {
	.poll_interval = 60,
};

static struct ovs_monitor ovs_fdb_mon = {
	.type = OVS_MON_FDB,
	.fd = -1,
};

static struct ovs_monitor ovs_arp_mon = {
	.type = OVS_MON_ARP,
	.fd = -1,
};

static struct event *ovs_poll_timer;
static struct hash *ovs_ofport_hash;
static struct hash *ovs_route_hash;
static struct hash *ovs_nh_hash;
static struct hash *ovs_fdb_hash;
static struct hash *ovs_arp_hash;

static void ovs_ofport_free(void *data);

static unsigned int ovs_ofport_hash_key(const void *arg)
{
	const struct ovs_ofport_map *map = arg;

	return map->ofport;
}

static bool ovs_ofport_hash_equal(const void *arg1, const void *arg2)
{
	const struct ovs_ofport_map *a = arg1;
	const struct ovs_ofport_map *b = arg2;

	return a->ofport == b->ofport;
}

static unsigned int ovs_key_hash_key(const void *arg)
{
	const struct ovs_key_entry *entry = arg;

	return string_hash_make(entry->key);
}

static bool ovs_key_hash_equal(const void *arg1, const void *arg2)
{
	const struct ovs_key_entry *a = arg1;
	const struct ovs_key_entry *b = arg2;

	return strcmp(a->key, b->key) == 0;
}

static unsigned int ovs_flow_hash_key(const void *arg)
{
	const struct ovs_key_entry *entry = arg;

	return string_hash_make(entry->key);
}

static bool ovs_flow_hash_equal(const void *arg1, const void *arg2)
{
	const struct ovs_key_entry *a = arg1;
	const struct ovs_key_entry *b = arg2;

	return strcmp(a->key, b->key) == 0;
}

static const char *ovs_ofctl_path(void)
{
	if (ovs_cfg.ofctl_path)
		return ovs_cfg.ofctl_path;
	return "ovs-ofctl";
}

bool zebra_ovs_is_enabled(void)
{
	return ovs_cfg.enabled;
}

void zebra_ovs_set_enabled(bool enabled)
{
	ovs_cfg.enabled = enabled;
}

void zebra_ovs_set_fdb_bridge(const char *bridge)
{
	XFREE(MTYPE_OVS, ovs_cfg.fdb_bridge);
	if (bridge && bridge[0])
		ovs_cfg.fdb_bridge = XSTRDUP(MTYPE_OVS, bridge);
}

void zebra_ovs_set_arp_bridge(const char *bridge)
{
	XFREE(MTYPE_OVS, ovs_cfg.arp_bridge);
	if (bridge && bridge[0])
		ovs_cfg.arp_bridge = XSTRDUP(MTYPE_OVS, bridge);
}

void zebra_ovs_set_ofctl_path(const char *path)
{
	XFREE(MTYPE_OVS, ovs_cfg.ofctl_path);
	if (path && path[0])
		ovs_cfg.ofctl_path = XSTRDUP(MTYPE_OVS, path);
}

void zebra_ovs_set_poll_interval(uint32_t seconds)
{
	ovs_cfg.poll_interval = seconds;
}

static void ovs_ofport_hash_reset(void)
{
	if (!ovs_ofport_hash) {
		ovs_ofport_hash = hash_create(ovs_ofport_hash_key,
					      ovs_ofport_hash_equal,
					      "OVS ofport map");
		return;
	}

	hash_clean(ovs_ofport_hash, ovs_ofport_free);
}

static void ovs_key_hash_init(void)
{
	if (!ovs_route_hash)
		ovs_route_hash = hash_create(ovs_key_hash_key,
					     ovs_key_hash_equal,
					     "OVS route map");
	if (!ovs_nh_hash)
		ovs_nh_hash = hash_create(ovs_key_hash_key,
					  ovs_key_hash_equal,
					  "OVS nh map");
	if (!ovs_fdb_hash)
		ovs_fdb_hash = hash_create(ovs_flow_hash_key,
					   ovs_flow_hash_equal,
					   "OVS fdb map");
	if (!ovs_arp_hash)
		ovs_arp_hash = hash_create(ovs_flow_hash_key,
					   ovs_flow_hash_equal,
					   "OVS arp map");
}

static struct interface *ovs_ifp_from_ofport(uint32_t ofport)
{
	struct ovs_ofport_map lookup, *found;

	if (!ovs_ofport_hash)
		return NULL;

	lookup.ofport = ofport;
	found = hash_lookup(ovs_ofport_hash, &lookup);
	if (!found)
		return NULL;
	return found->ifp;
}

static char *ovs_cmd_capture(char *const argv[])
{
	int pipefd[2];
	pid_t pid;
	ssize_t nread;
	size_t used = 0;
	size_t cap = 8192;
	char *buf = NULL;

	if (pipe(pipefd) < 0)
		return NULL;

	pid = fork();
	if (pid < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		return NULL;
	}

	if (pid == 0) {
		dup2(pipefd[1], STDOUT_FILENO);
		dup2(pipefd[1], STDERR_FILENO);
		close(pipefd[0]);
		close(pipefd[1]);
		execvp(argv[0], argv);
		_exit(127);
	}

	close(pipefd[1]);

	buf = XMALLOC(MTYPE_OVS_BUF, cap);
	while ((nread = read(pipefd[0], buf + used, cap - used - 1)) > 0) {
		used += (size_t)nread;
		if (cap - used <= 1) {
			cap *= 2;
			buf = XREALLOC(MTYPE_OVS_BUF, buf, cap);
		}
	}
	close(pipefd[0]);
	buf[used] = '\0';

	waitpid(pid, NULL, 0);
	return buf;
}

static bool ovs_json_is_tagged(struct json_object *obj, const char *tag)
{
	struct json_object *tag_obj;

	if (!obj || !json_object_is_type(obj, json_type_array))
		return false;
	if (json_object_array_length(obj) < 2)
		return false;
	tag_obj = json_object_array_get_idx(obj, 0);
	if (!tag_obj || !json_object_is_type(tag_obj, json_type_string))
		return false;
	return strcmp(json_object_get_string(tag_obj), tag) == 0;
}

static struct json_object *ovs_json_set_first(struct json_object *obj)
{
	struct json_object *vals;

	if (!ovs_json_is_tagged(obj, "set"))
		return obj;
	vals = json_object_array_get_idx(obj, 1);
	if (!vals || !json_object_is_type(vals, json_type_array))
		return NULL;
	if (json_object_array_length(vals) == 0)
		return NULL;
	return json_object_array_get_idx(vals, 0);
}

static const char *ovs_json_string(struct json_object *obj)
{
	obj = ovs_json_set_first(obj);
	if (!obj || !json_object_is_type(obj, json_type_string))
		return NULL;
	return json_object_get_string(obj);
}

static int64_t ovs_json_int(struct json_object *obj, int64_t defval)
{
	obj = ovs_json_set_first(obj);
	if (!obj || !json_object_is_type(obj, json_type_int))
		return defval;
	return json_object_get_int64(obj);
}

static struct json_object *ovs_json_map_pairs(struct json_object *obj)
{
	if (!ovs_json_is_tagged(obj, "map"))
		return NULL;
	return json_object_array_get_idx(obj, 1);
}

static const char *ovs_map_get_string(struct json_object *map,
				      const char *key)
{
	struct json_object *pairs;
	int i;

	pairs = ovs_json_map_pairs(map);
	if (!pairs || !json_object_is_type(pairs, json_type_array))
		return NULL;

	for (i = 0; i < json_object_array_length(pairs); i++) {
		struct json_object *pair = json_object_array_get_idx(pairs, i);
		struct json_object *k;
		struct json_object *v;

		if (!pair || !json_object_is_type(pair, json_type_array))
			continue;
		if (json_object_array_length(pair) != 2)
			continue;
		k = json_object_array_get_idx(pair, 0);
		v = json_object_array_get_idx(pair, 1);
		if (!k || !json_object_is_type(k, json_type_string))
			continue;
		if (strcmp(json_object_get_string(k), key) == 0)
			return json_object_get_string(v);
	}
	return NULL;
}

static struct list *ovs_query_interfaces(void)
{
	char *json_str = NULL;
	struct json_object *root = NULL;
	struct json_object *headings = NULL;
	struct json_object *data = NULL;
	struct list *iflist = NULL;
	int idx_name = -1;
	int idx_ofport = -1;
	int idx_mac = -1;
	int idx_mtu = -1;
	int idx_admin = -1;
	int idx_link = -1;
	int idx_ext = -1;
	int idx_type = -1;
	int i;

	char *const argv[] = {
		"ovs-vsctl",
		"--format=json",
		"--columns=name,ofport,mac_in_use,mtu,admin_state,link_state,external_ids,type",
		"list",
		"Interface",
		NULL,
	};

	json_str = ovs_cmd_capture(argv);
	if (!json_str || json_str[0] == '\0')
		goto out;

	root = json_tokener_parse(json_str);
	if (!root)
		goto out;

	headings = json_object_object_get(root, "headings");
	data = json_object_object_get(root, "data");
	if (!headings || !data)
		goto out;

	for (i = 0; i < json_object_array_length(headings); i++) {
		const char *h = json_object_get_string(
			json_object_array_get_idx(headings, i));

		if (!h)
			continue;
		if (strcmp(h, "name") == 0)
			idx_name = i;
		else if (strcmp(h, "ofport") == 0)
			idx_ofport = i;
		else if (strcmp(h, "mac_in_use") == 0)
			idx_mac = i;
		else if (strcmp(h, "mtu") == 0)
			idx_mtu = i;
		else if (strcmp(h, "admin_state") == 0)
			idx_admin = i;
		else if (strcmp(h, "link_state") == 0)
			idx_link = i;
		else if (strcmp(h, "external_ids") == 0)
			idx_ext = i;
		else if (strcmp(h, "type") == 0)
			idx_type = i;
	}

	if (idx_name < 0 || idx_ofport < 0 || idx_ext < 0)
		goto out;

	iflist = list_new();

	for (i = 0; i < json_object_array_length(data); i++) {
		struct json_object *row = json_object_array_get_idx(data, i);
		struct ovs_ifinfo *info;
		const char *name;
		const char *mac;
		const char *admin;
		const char *link;
		const char *ext_ifindex;
		const char *ext_mtu;
		const char *ext_mac;
		const char *ext_admin;
		const char *ext_link;
		const char *ext_bridge;
		const char *ext_bond;
		const char *ext_esi;
		const char *ext_vni;
		const char *ext_vtep;
		const char *ext_mcast;
		const char *ext_access_vlan;
		const char *type;
		struct json_object *ext;

		if (!row || !json_object_is_type(row, json_type_array))
			continue;

		name = ovs_json_string(json_object_array_get_idx(row, idx_name));
		if (!name || name[0] == '\0')
			continue;

		info = XCALLOC(MTYPE_OVS, sizeof(*info));
		info->name = XSTRDUP(MTYPE_OVS, name);

		info->ofport = ovs_json_int(
			json_object_array_get_idx(row, idx_ofport), -1);
		info->mtu = ovs_json_int(
			json_object_array_get_idx(row, idx_mtu), -1);
		mac = ovs_json_string(json_object_array_get_idx(row, idx_mac));
		admin = ovs_json_string(
			json_object_array_get_idx(row, idx_admin));
		link = ovs_json_string(json_object_array_get_idx(row, idx_link));
		type = idx_type >= 0
			       ? ovs_json_string(json_object_array_get_idx(
					 row, idx_type))
			       : NULL;

		ext = json_object_array_get_idx(row, idx_ext);
		ext_ifindex = ovs_map_get_string(ext, "zebra.ifindex");
		ext_mtu = ovs_map_get_string(ext, "zebra.mtu");
		ext_mac = ovs_map_get_string(ext, "zebra.mac");
		ext_admin = ovs_map_get_string(ext, "zebra.admin_state");
		ext_link = ovs_map_get_string(ext, "zebra.link_state");
		ext_bridge = ovs_map_get_string(ext, "zebra.bridge");
		ext_bond = ovs_map_get_string(ext, "zebra.bond");
		ext_esi = ovs_map_get_string(ext, "zebra.esi");
		ext_vni = ovs_map_get_string(ext, "zebra.vni");
		ext_vtep = ovs_map_get_string(ext, "zebra.vtep_ip");
		ext_mcast = ovs_map_get_string(ext, "zebra.mcast_grp");
		ext_access_vlan = ovs_map_get_string(ext, "zebra.access_vlan");

		if (ext_ifindex)
			info->ifindex = strtoll(ext_ifindex, NULL, 10);
		else
			info->ifindex = info->ofport;

		if (ext_mtu)
			info->mtu = strtoll(ext_mtu, NULL, 10);

		if (ext_mac)
			info->mac = XSTRDUP(MTYPE_OVS, ext_mac);
		else if (mac)
			info->mac = XSTRDUP(MTYPE_OVS, mac);

		if (ext_admin)
			admin = ext_admin;
		if (ext_link)
			link = ext_link;

		info->admin_up = admin && strcmp(admin, "up") == 0;
		info->link_up = link && strcmp(link, "up") == 0;

		if (ext_bridge && ext_bridge[0])
			info->bridge_name = XSTRDUP(MTYPE_OVS, ext_bridge);

		info->is_bridge = (strncmp(info->name, "B_", 2) == 0);
		if (type && strcmp(type, "vxlan") == 0)
			info->is_vxlan = true;
		if (ext_vni || ext_vtep || ext_mcast || ext_access_vlan)
			info->is_vxlan = true;
		if (ext_esi && ext_esi[0]) {
			info->esi = XSTRDUP(MTYPE_OVS, ext_esi);
			info->is_bond = true;
		}
		if (ext_bond && ext_bond[0]) {
			info->bond_master = XSTRDUP(MTYPE_OVS, ext_bond);
		}
		if (ext_vni && zebra_ovs_parse_vni(ext_vni, &info->vni))
			info->has_vni = true;
		if (ext_access_vlan &&
		    zebra_ovs_parse_vlan(ext_access_vlan, &info->access_vlan))
			info->has_access_vlan = true;
		if (ext_vtep && zebra_ovs_parse_ipv4(ext_vtep, &info->vtep_ip))
			info->has_vtep_ip = true;
		if (ext_mcast &&
		    zebra_ovs_parse_ipv4(ext_mcast, &info->mcast_grp))
			info->has_mcast_grp = true;

		listnode_add(iflist, info);
	}

out:
	if (root)
		json_object_put(root);
	if (json_str)
		XFREE(MTYPE_OVS_BUF, json_str);
	return iflist;
}

static void ovs_ifinfo_free(void *arg)
{
	struct ovs_ifinfo *info = arg;

	if (!info)
		return;
	XFREE(MTYPE_OVS, info->name);
	XFREE(MTYPE_OVS, info->bridge_name);
	XFREE(MTYPE_OVS, info->mac);
	XFREE(MTYPE_OVS, info->bond_master);
	XFREE(MTYPE_OVS, info->esi);
	XFREE(MTYPE_OVS, info);
}

static void ovs_ofport_free(void *data)
{
	struct ovs_ofport_map *map = data;

	XFREE(MTYPE_OVS, map);
}

static void ovs_fdb_free(void *data)
{
	struct ovs_fdb_entry *entry = data;

	XFREE(MTYPE_OVS_KEY, entry->key);
	XFREE(MTYPE_OVS_KEY, entry);
}

static void ovs_arp_free(void *data)
{
	struct ovs_arp_entry *entry = data;

	XFREE(MTYPE_OVS_KEY, entry->key);
	XFREE(MTYPE_OVS_KEY, entry);
}

static void ovs_apply_ifinfo(struct zebra_ns *zns, struct ovs_ifinfo *info)
{
	struct interface *ifp;
	struct zebra_if *zif;
	struct zebra_l2info_bridge brinfo;
	struct interface *br_if = NULL;
	uint8_t macbuf[INTERFACE_HWADDR_MAX];
	int maclen = 0;
	enum zebra_iftype old_type;
	enum zebra_iftype new_type;
	enum zebra_slave_iftype old_slave;
	enum zebra_slave_iftype new_slave;
	ifindex_t old_bridge_ifindex;
	ifindex_t old_bond_ifindex;

	if (!info || !info->name)
		return;

	ifp = if_get_by_name(info->name, VRF_DEFAULT, NULL);
	if (!ifp)
		return;

	if (info->ifindex > 0)
		if_set_index(ifp, (ifindex_t)info->ifindex);

	ifp->flags = 0;
	if (info->admin_up)
		ifp->flags |= IFF_UP;
	if (info->link_up)
		ifp->flags |= IFF_RUNNING;

	if (info->mtu > 0) {
		if_update_state_mtu(ifp, (uint32_t)info->mtu);
		if_update_state_mtu6(ifp, (uint32_t)info->mtu);
	}

	if (info->mac && prefix_str2mac(info->mac, (struct ethaddr *)macbuf)) {
		maclen = ETH_ALEN;
		if_update_state_hw_addr(ifp, macbuf, maclen);
	}

	if_add_update(ifp);

	zif = ifp->info;
	if (!zif)
		return;

	old_type = zif->zif_type;
	old_slave = zif->zif_slave_type;
	old_bridge_ifindex = zif->brslave_info.bridge_ifindex;
	old_bond_ifindex = zif->bondslave_info.bond_ifindex;

	if (info->is_bridge)
		new_type = ZEBRA_IF_BRIDGE;
	else if (info->is_vxlan)
		new_type = ZEBRA_IF_VXLAN;
	else if (info->is_bond)
		new_type = ZEBRA_IF_BOND;
	else
		new_type = ZEBRA_IF_OTHER;

	if (old_type != new_type) {
		if (old_type == ZEBRA_IF_BRIDGE)
			zebra_l2_bridge_del(ifp);
		else if (old_type == ZEBRA_IF_VXLAN)
			zebra_l2_vxlanif_del(ifp);
		else if (old_type == ZEBRA_IF_BOND)
			zebra_l2if_update_bond(ifp, false);
	}

	zif->zif_type = new_type;

	new_slave = ZEBRA_IF_SLAVE_NONE;
	if (info->bond_master && info->bond_master[0])
		new_slave = ZEBRA_IF_SLAVE_BOND;
	else if (info->bridge_name && info->bridge_name[0])
		new_slave = ZEBRA_IF_SLAVE_BRIDGE;
	zif->zif_slave_type = new_slave;

	if (info->is_bridge) {
		zebra_evpn_if_init(zif);
		memset(&brinfo, 0, sizeof(brinfo));
		brinfo.bridge.vlan_aware = 0;
		zebra_l2_bridge_add_update(ifp, &brinfo);
		return;
	}

	if (new_type == ZEBRA_IF_VXLAN) {
		struct zebra_l2info_vxlan vxl_info;
		bool add = (old_type != ZEBRA_IF_VXLAN);

		if (!info->has_vni) {
			zlog_warn("OVS vxlan interface %s missing zebra.vni",
				  info->name);
		} else {
			memset(&vxl_info, 0, sizeof(vxl_info));
			vxl_info.vni_info.iftype = ZEBRA_VXLAN_IF_VNI;
			vxl_info.vni_info.vni.vni = info->vni;
			if (info->has_access_vlan)
				vxl_info.vni_info.vni.access_vlan =
					info->access_vlan;
			if (info->has_mcast_grp)
				vxl_info.vni_info.vni.mcast_grp =
					info->mcast_grp;
			if (info->has_vtep_ip)
				vxl_info.vtep_ip = info->vtep_ip;

			zebra_l2_vxlanif_add_update(ifp, &vxl_info, add);

			if (!add && info->has_access_vlan &&
			    zif->l2info.vxl.vni_info.vni.access_vlan !=
				    info->access_vlan)
				zebra_l2_vxlanif_update_access_vlan(
					ifp, info->access_vlan);

			if (!info->has_vtep_ip)
				zlog_warn(
					"OVS vxlan interface %s missing zebra.vtep_ip",
					info->name);
			if (!info->has_access_vlan)
				zlog_warn(
					"OVS vxlan interface %s missing zebra.access_vlan",
					info->name);
		}
	}

	if (new_type == ZEBRA_IF_BOND) {
		zebra_l2if_update_bond(ifp, old_type != ZEBRA_IF_BOND);
		zebra_evpn_if_init(zif);
		if (info->esi && info->esi[0]) {
			esi_t esi;

			if (str_to_esi(info->esi, &esi))
				zebra_evpn_es_type0_esi_update(zif, &esi);
			else
				zlog_warn(
					"OVS bond %s has invalid zebra.esi '%s'",
					info->name, info->esi);
		} else {
			zebra_evpn_es_type0_esi_update(zif, NULL);
		}

	}

	if (info->bond_master && info->bond_master[0]) {
		char *endptr = NULL;
		unsigned long bond_ifindex =
			strtoul(info->bond_master, &endptr, 10);

		if (endptr && *endptr == '\0') {
			zebra_l2if_update_bond_slave(
				ifp, (ifindex_t)bond_ifindex, false);
		} else {
			struct interface *bond_if = if_lookup_by_name_per_ns(
				zns, info->bond_master);

			if (bond_if)
				zebra_l2if_update_bond_slave(
					ifp, bond_if->ifindex, false);
			else
				zebra_l2if_update_bond_slave(
					ifp, IFINDEX_INTERNAL, false);
		}
	} else if (old_slave == ZEBRA_IF_SLAVE_BOND &&
		   old_bond_ifindex != IFINDEX_INTERNAL) {
		zebra_l2if_update_bond_slave(ifp, IFINDEX_INTERNAL, false);
	}

	if (info->bridge_name && info->bridge_name[0]) {
		br_if = if_lookup_by_name_per_ns(zns, info->bridge_name);
		if (!br_if) {
			char *endptr = NULL;
			unsigned long br_ifindex =
				strtoul(info->bridge_name, &endptr, 10);

			if (endptr && *endptr == '\0')
				zebra_l2if_update_bridge_slave(
					ifp, (ifindex_t)br_ifindex,
					zns->ns_id, ZEBRA_BRIDGE_NO_ACTION);
		} else {
			zebra_l2if_update_bridge_slave(
				ifp, br_if->ifindex, zns->ns_id,
				ZEBRA_BRIDGE_NO_ACTION);
		}
	} else if (old_slave == ZEBRA_IF_SLAVE_BRIDGE &&
		   old_bridge_ifindex != IFINDEX_INTERNAL) {
		zebra_l2if_update_bridge_slave(ifp, IFINDEX_INTERNAL, zns->ns_id,
					       ZEBRA_BRIDGE_NO_ACTION);
	}

	if (new_type == ZEBRA_IF_BOND) {
		if (info->has_access_vlan)
			zebra_evpn_vl_mbr_ref(info->access_vlan, zif);
		else
			zlog_warn(
				"OVS bond %s missing zebra.access_vlan for EVPN-MH",
				info->name);
	}
}

void zebra_ovs_interface_list(struct zebra_ns *zns)
{
	struct list *iflist;
	struct listnode *node;
	struct ovs_ifinfo *info;

	if (!zebra_ovs_is_enabled())
		return;

	iflist = ovs_query_interfaces();
	if (!iflist)
		return;

	ovs_ofport_hash_reset();

	for (ALL_LIST_ELEMENTS_RO(iflist, node, info)) {
		ovs_apply_ifinfo(zns, info);
		if (info->ofport > 0) {
			struct ovs_ofport_map *map =
				XCALLOC(MTYPE_OVS, sizeof(*map));
			struct ovs_ofport_map *stored;

			map->ofport = (uint32_t)info->ofport;
			map->ifp = if_lookup_by_name_per_ns(zns, info->name);
			if (map->ifp) {
				stored = hash_get(ovs_ofport_hash, map,
						  hash_alloc_intern);
				if (stored != map)
					XFREE(MTYPE_OVS, map);
			} else {
				XFREE(MTYPE_OVS, map);
			}
		}
	}

	iflist->del = ovs_ifinfo_free;
	list_delete_all_node(iflist);
	list_delete(&iflist);
}

void zebra_ovs_interface_list_tunneldump(struct zebra_ns *zns)
{
	if (!zebra_ovs_is_enabled())
		return;

	/* Re-scan OVS interfaces for tunnel metadata (vxlan) if needed. */
	zebra_ovs_interface_list(zns);
}

void zebra_ovs_interface_list_second(struct zebra_ns *zns)
{
	if (!zebra_ovs_is_enabled())
		return;

	zebra_if_update_all_links(zns);
}

static void ovs_apply_fdb_entry(const struct ethaddr *mac, uint32_t ofport,
				bool is_del)
{
	struct interface *ifp;
	struct zebra_if *zif;
	struct interface *br_if;

	if (!is_evpn_enabled())
		return;

	ifp = ovs_ifp_from_ofport(ofport);
	if (!ifp || !ifp->info)
		return;

	zif = ifp->info;
	br_if = zif->brslave_info.br_if;
	if (!br_if)
		return;

	if (is_del)
		zebra_vxlan_local_mac_del(ifp, br_if, mac, 0);
	else
		zebra_vxlan_local_mac_add_update(ifp, br_if, mac, 0, false,
						 false, false);
}

static void ovs_apply_arp_entry(const struct ipaddr *ip,
				const struct ethaddr *mac, uint32_t ofport,
				bool is_del)
{
	struct interface *ifp;
	struct zebra_if *zif;
	struct interface *br_if;
	union sockunion lladdr;
	int llalen = 0;

	if (!is_evpn_enabled())
		return;

	ifp = ovs_ifp_from_ofport(ofport);
	if (!ifp || !ifp->info)
		return;

	zif = ifp->info;
	br_if = zif->brslave_info.br_if;
	if (!br_if)
		return;

	if (!is_del) {
		zebra_neigh_add(ifp, ip, mac);
		zebra_vxlan_handle_kernel_neigh_update(
			ifp, br_if, ip, mac, NUD_REACHABLE, false, false,
			false, false);

		memset(&lladdr, 0, sizeof(lladdr));
		sockunion_family(&lladdr) = AF_INET;
		memcpy(sockunion_get_addr(&lladdr), mac, ETH_ALEN);
		llalen = ETH_ALEN;
		zsend_neighbor_notify(ZEBRA_NEIGH_ADDED, ifp, ip, 0, &lladdr,
				      llalen);
		return;
	}

	zebra_neigh_del(ifp, ip);
	zebra_vxlan_handle_kernel_neigh_del(ifp, br_if, ip);

	memset(&lladdr, 0, sizeof(lladdr));
	sockunion_family(&lladdr) = AF_INET;
	memcpy(sockunion_get_addr(&lladdr), mac, ETH_ALEN);
	llalen = ETH_ALEN;
	zsend_neighbor_notify(ZEBRA_NEIGH_REMOVED, ifp, ip, 0, &lladdr, llalen);
}

static void ovs_handle_fdb_event(const char *line, bool is_del)
{
	struct ethaddr mac;
	uint32_t ofport;

	if (!zebra_ovs_parse_fdb_line(line, &mac, &ofport))
		return;

	ovs_apply_fdb_entry(&mac, ofport, is_del);
}

static void ovs_handle_arp_event(const char *line, bool is_del)
{
	struct ipaddr ip;
	struct ethaddr mac;
	uint32_t ofport;

	if (!zebra_ovs_parse_arp_line(line, &ip, &mac, &ofport))
		return;

	ovs_apply_arp_entry(&ip, &mac, ofport, is_del);
}

static void ovs_monitor_line(enum ovs_monitor_type type, const char *line)
{
	bool is_del = zebra_ovs_parse_event_is_del(line);

	if (type == OVS_MON_FDB)
		ovs_handle_fdb_event(line, is_del);
	else
		ovs_handle_arp_event(line, is_del);
}

static struct ovs_fdb_entry *ovs_parse_fdb_line(const char *line)
{
	struct ovs_fdb_entry *entry;

	entry = XCALLOC(MTYPE_OVS_KEY, sizeof(*entry));
	if (!zebra_ovs_parse_fdb_line(line, &entry->mac, &entry->ofport)) {
		XFREE(MTYPE_OVS_KEY, entry);
		return NULL;
	}

	entry->key = asprintfrr(MTYPE_OVS_KEY, "%pEA:%u", &entry->mac,
				entry->ofport);
	return entry;
}

static struct ovs_arp_entry *ovs_parse_arp_line(const char *line)
{
	char macbuf[64];
	struct ovs_arp_entry *entry;

	entry = XCALLOC(MTYPE_OVS_KEY, sizeof(*entry));
	if (!zebra_ovs_parse_arp_line(line, &entry->ip, &entry->mac,
				      &entry->ofport)) {
		XFREE(MTYPE_OVS_KEY, entry);
		return NULL;
	}

	if (!zebra_ovs_parse_token(line, "arp_sha", macbuf, sizeof(macbuf)) &&
	    !zebra_ovs_parse_token(line, "dl_src", macbuf, sizeof(macbuf)))
		macbuf[0] = '\0';

	entry->key = asprintfrr(MTYPE_OVS_KEY, "%pIA:%u:%s", &entry->ip,
				entry->ofport, macbuf);
	return entry;
}

static void ovs_reconcile_fdb_del(struct hash_bucket *hb, void *arg)
{
	struct hash *new_hash = arg;
	struct ovs_fdb_entry *entry = hb->data;

	if (!hash_lookup(new_hash, entry))
		ovs_apply_fdb_entry(&entry->mac, entry->ofport, true);
}

static void ovs_reconcile_fdb_add(struct hash_bucket *hb, void *arg)
{
	struct hash *old_hash = arg;
	struct ovs_fdb_entry *entry = hb->data;

	if (!old_hash || !hash_lookup(old_hash, entry))
		ovs_apply_fdb_entry(&entry->mac, entry->ofport, false);
}

static void ovs_reconcile_arp_del(struct hash_bucket *hb, void *arg)
{
	struct hash *new_hash = arg;
	struct ovs_arp_entry *entry = hb->data;

	if (!hash_lookup(new_hash, entry))
		ovs_apply_arp_entry(&entry->ip, &entry->mac, entry->ofport,
				    true);
}

static void ovs_reconcile_arp_add(struct hash_bucket *hb, void *arg)
{
	struct hash *old_hash = arg;
	struct ovs_arp_entry *entry = hb->data;

	if (!old_hash || !hash_lookup(old_hash, entry))
		ovs_apply_arp_entry(&entry->ip, &entry->mac, entry->ofport,
				    false);
}

static void ovs_monitor_read(struct event *thread)
{
	struct ovs_monitor *mon = EVENT_ARG(thread);
	ssize_t nread;
	char *newline;

	if (!mon || mon->fd < 0)
		return;

	nread = read(mon->fd, mon->buf + mon->len,
		     sizeof(mon->buf) - mon->len - 1);
	if (nread <= 0) {
		close(mon->fd);
		mon->fd = -1;
		return;
	}

	mon->len += (size_t)nread;
	mon->buf[mon->len] = '\0';

	while ((newline = strchr(mon->buf, '\n')) != NULL) {
		*newline = '\0';
		ovs_monitor_line(mon->type, mon->buf);
		mon->len -= (size_t)(newline - mon->buf + 1);
		memmove(mon->buf, newline + 1, mon->len);
		mon->buf[mon->len] = '\0';
	}

	event_add_read(zrouter.master, ovs_monitor_read, mon, mon->fd,
		       &mon->t_read);
}

static int ovs_spawn_monitor(struct ovs_monitor *mon)
{
	int pipefd[2];
	pid_t pid;
	int flags;
	char *const argv[] = {
		(char *)ovs_ofctl_path(),
		"monitor",
		mon->bridge,
		NULL,
	};

	if (!mon->bridge || mon->bridge[0] == '\0')
		return -1;

	if (pipe(pipefd) < 0)
		return -1;

	pid = fork();
	if (pid < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		return -1;
	}

	if (pid == 0) {
		dup2(pipefd[1], STDOUT_FILENO);
		dup2(pipefd[1], STDERR_FILENO);
		close(pipefd[0]);
		close(pipefd[1]);
		execvp(argv[0], argv);
		_exit(127);
	}

	close(pipefd[1]);
	mon->pid = pid;
	mon->fd = pipefd[0];

	flags = fcntl(mon->fd, F_GETFL, 0);
	if (flags >= 0)
		fcntl(mon->fd, F_SETFL, flags | O_NONBLOCK);

	mon->len = 0;
	event_add_read(zrouter.master, ovs_monitor_read, mon, mon->fd,
		       &mon->t_read);
	return 0;
}

static void ovs_monitor_stop(struct ovs_monitor *mon)
{
	if (!mon)
		return;
	if (mon->fd >= 0) {
		close(mon->fd);
		mon->fd = -1;
	}
	if (mon->pid > 0) {
		kill(mon->pid, SIGTERM);
		waitpid(mon->pid, NULL, 0);
		mon->pid = -1;
	}
}

static void ovs_poll_dump_flows(const char *bridge,
				enum ovs_monitor_type type)
{
	FILE *fp;
	char line[4096];
	char cmd[256];
	struct hash *new_hash;
	struct hash *old_hash;

	if (!bridge || bridge[0] == '\0')
		return;

	snprintf(cmd, sizeof(cmd), "%s dump-flows %s", ovs_ofctl_path(),
		 bridge);
	fp = popen(cmd, "r");
	if (!fp)
		return;

	new_hash = hash_create(ovs_flow_hash_key, ovs_flow_hash_equal,
			       type == OVS_MON_FDB ? "OVS fdb poll"
						   : "OVS arp poll");

	while (fgets(line, sizeof(line), fp) != NULL) {
		if (type == OVS_MON_FDB) {
			struct ovs_fdb_entry *entry;
			struct ovs_fdb_entry *stored;

			entry = ovs_parse_fdb_line(line);
			if (!entry)
				continue;
			stored = hash_get(new_hash, entry, hash_alloc_intern);
			if (stored != entry)
				ovs_fdb_free(entry);
		} else {
			struct ovs_arp_entry *entry;
			struct ovs_arp_entry *stored;

			entry = ovs_parse_arp_line(line);
			if (!entry)
				continue;
			stored = hash_get(new_hash, entry, hash_alloc_intern);
			if (stored != entry)
				ovs_arp_free(entry);
		}
	}

	pclose(fp);

	if (type == OVS_MON_FDB) {
		old_hash = ovs_fdb_hash;
		if (old_hash)
			hash_iterate(old_hash, ovs_reconcile_fdb_del,
				     new_hash);
		hash_iterate(new_hash, ovs_reconcile_fdb_add, old_hash);
		if (old_hash)
			hash_clean_and_free(&old_hash, ovs_fdb_free);
		ovs_fdb_hash = new_hash;
	} else {
		old_hash = ovs_arp_hash;
		if (old_hash)
			hash_iterate(old_hash, ovs_reconcile_arp_del,
				     new_hash);
		hash_iterate(new_hash, ovs_reconcile_arp_add, old_hash);
		if (old_hash)
			hash_clean_and_free(&old_hash, ovs_arp_free);
		ovs_arp_hash = new_hash;
	}
}

static void ovs_poll_timer_cb(struct event *thread)
{
	(void)thread;

	if (!zebra_ovs_is_enabled())
		return;

	ovs_poll_dump_flows(ovs_cfg.fdb_bridge, OVS_MON_FDB);
	ovs_poll_dump_flows(ovs_cfg.arp_bridge, OVS_MON_ARP);

	if (ovs_cfg.poll_interval > 0)
		event_add_timer(zrouter.master, ovs_poll_timer_cb, NULL,
				ovs_cfg.poll_interval, &ovs_poll_timer);
}

void zebra_ovs_init(struct zebra_ns *zns)
{
	(void)zns;

	if (!zebra_ovs_is_enabled())
		return;

	if (!ovs_cfg.fdb_bridge || !ovs_cfg.arp_bridge)
		zlog_warn("OVS enabled but FDB/ARP bridge names are not fully configured");

	ovs_key_hash_init();

	ovs_fdb_mon.bridge = ovs_cfg.fdb_bridge;
	ovs_arp_mon.bridge = ovs_cfg.arp_bridge;

	if (ovs_cfg.fdb_bridge)
		ovs_spawn_monitor(&ovs_fdb_mon);
	if (ovs_cfg.arp_bridge)
		ovs_spawn_monitor(&ovs_arp_mon);

	if (ovs_cfg.poll_interval > 0)
		event_add_timer(zrouter.master, ovs_poll_timer_cb, NULL,
				ovs_cfg.poll_interval, &ovs_poll_timer);
}

void zebra_ovs_terminate(struct zebra_ns *zns)
{
	(void)zns;

	if (!zebra_ovs_is_enabled())
		return;

	ovs_monitor_stop(&ovs_fdb_mon);
	ovs_monitor_stop(&ovs_arp_mon);
	if (ovs_poll_timer)
		EVENT_OFF(ovs_poll_timer);
}

static int ovs_exec_ofctl(char *const argv[])
{
	pid_t pid;
	int status;

	pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0) {
		execvp(argv[0], argv);
		_exit(127);
	}
	if (waitpid(pid, &status, 0) < 0)
		return -1;
	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	return -1;
}

static void ovs_program_mac(struct zebra_dplane_ctx *ctx)
{
	const struct ethaddr *mac;
	char flow[256];
	char match[128];
	char *const add_argv[] = {(char *)ovs_ofctl_path(), "add-flow",
				  ovs_cfg.fdb_bridge, flow, NULL};
	char *const del_argv[] = {(char *)ovs_ofctl_path(), "del-flows",
				  ovs_cfg.fdb_bridge, match, NULL};
	bool is_del;
	uint32_t ofport;

	if (!ovs_cfg.fdb_bridge)
		return;

	mac = dplane_ctx_mac_get_addr(ctx);
	if (!mac)
		return;

	is_del = (dplane_ctx_get_op(ctx) == DPLANE_OP_MAC_DELETE);
	ofport = (uint32_t)dplane_ctx_get_ifindex(ctx);

	snprintf(match, sizeof(match), "dl_dst=%pEA", mac);
	snprintf(flow, sizeof(flow),
		 "table=0,priority=200,dl_dst=%pEA,actions=output:%u", mac,
		 ofport);

	if (is_del)
		ovs_exec_ofctl(del_argv);
	else
		ovs_exec_ofctl(add_argv);
}

static void ovs_program_neigh(struct zebra_dplane_ctx *ctx)
{
	const struct ipaddr *ip;
	char ipbuf[64];
	char flow[256];
	char match[128];
	char *const add_argv[] = {(char *)ovs_ofctl_path(), "add-flow",
				  ovs_cfg.arp_bridge, flow, NULL};
	char *const del_argv[] = {(char *)ovs_ofctl_path(), "del-flows",
				  ovs_cfg.arp_bridge, match, NULL};
	bool is_del;
	uint32_t ofport;

	if (!ovs_cfg.arp_bridge)
		return;

	ip = dplane_ctx_neigh_get_ipaddr(ctx);
	if (!ip)
		return;

	ipaddr2str(ip, ipbuf, sizeof(ipbuf));
	is_del = (dplane_ctx_get_op(ctx) == DPLANE_OP_NEIGH_DELETE ||
		  dplane_ctx_get_op(ctx) == DPLANE_OP_NEIGH_IP_DELETE);
	ofport = (uint32_t)dplane_ctx_get_ifindex(ctx);

	snprintf(match, sizeof(match), "arp,arp_tpa=%s", ipbuf);
	snprintf(flow, sizeof(flow),
		 "table=0,priority=200,arp,arp_tpa=%s,actions=output:%u",
		 ipbuf, ofport);

	if (is_del)
		ovs_exec_ofctl(del_argv);
	else
		ovs_exec_ofctl(add_argv);
}

static void ovs_record_key(struct hash *h, const char *key, bool add)
{
	struct ovs_key_entry lookup, *entry;

	if (!h || !key)
		return;

	lookup.key = (char *)key;
	entry = hash_lookup(h, &lookup);
	if (add) {
		if (!entry) {
			entry = XCALLOC(MTYPE_OVS_KEY, sizeof(*entry));
			entry->key = XSTRDUP(MTYPE_OVS_KEY, key);
			hash_get(h, entry, hash_alloc_intern);
		}
		return;
	}

	if (entry) {
		hash_release(h, entry);
		XFREE(MTYPE_OVS_KEY, entry->key);
		XFREE(MTYPE_OVS_KEY, entry);
	}
}

static void ovs_record_route(struct zebra_dplane_ctx *ctx)
{
	char buf[PREFIX_STRLEN + 32];
	const struct prefix *p = dplane_ctx_get_dest(ctx);
	uint32_t table = dplane_ctx_get_table(ctx);
	bool add = dplane_ctx_get_op(ctx) != DPLANE_OP_ROUTE_DELETE;

	if (!p)
		return;

	snprintf(buf, sizeof(buf), "%u:%pFX", table, p);
	ovs_record_key(ovs_route_hash, buf, add);
}

static void ovs_record_nh(struct zebra_dplane_ctx *ctx)
{
	char buf[64];
	uint32_t id = dplane_ctx_get_nhe_id(ctx);
	bool add = dplane_ctx_get_op(ctx) != DPLANE_OP_NH_DELETE;

	snprintf(buf, sizeof(buf), "nhg:%u", id);
	ovs_record_key(ovs_nh_hash, buf, add);
}

void zebra_ovs_update_multi(struct dplane_ctx_list_head *ctx_list)
{
	struct zebra_dplane_ctx *ctx;
	struct dplane_ctx_list_head handled_list;

	dplane_ctx_q_init(&handled_list);

	while ((ctx = dplane_ctx_dequeue(ctx_list)) != NULL) {
		dplane_ctx_set_status(ctx, ZEBRA_DPLANE_REQUEST_SUCCESS);

		switch (dplane_ctx_get_op(ctx)) {
		case DPLANE_OP_ROUTE_INSTALL:
		case DPLANE_OP_ROUTE_UPDATE:
		case DPLANE_OP_ROUTE_DELETE:
			ovs_record_route(ctx);
			break;
		case DPLANE_OP_NH_INSTALL:
		case DPLANE_OP_NH_UPDATE:
		case DPLANE_OP_NH_DELETE:
			ovs_record_nh(ctx);
			break;
		case DPLANE_OP_MAC_INSTALL:
		case DPLANE_OP_MAC_DELETE:
			ovs_program_mac(ctx);
			break;
		case DPLANE_OP_NEIGH_INSTALL:
		case DPLANE_OP_NEIGH_UPDATE:
		case DPLANE_OP_NEIGH_DELETE:
		case DPLANE_OP_NEIGH_IP_INSTALL:
		case DPLANE_OP_NEIGH_IP_DELETE:
			ovs_program_neigh(ctx);
			break;
		default:
			break;
		}

		dplane_ctx_enqueue_tail(&handled_list, ctx);
	}

	dplane_ctx_q_init(ctx_list);
	dplane_ctx_list_append(ctx_list, &handled_list);
}
