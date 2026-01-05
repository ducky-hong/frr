// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * OVS dataplane provider.
 */

#include "zebra.h"

#include "zebra/ovs.h"
#include "zebra/zebra_dplane.h"

static int ovs_dplane_process_func(struct zebra_dplane_provider *prov)
{
	struct zebra_dplane_ctx *ctx;
	struct dplane_ctx_list_head work_list;
	int counter, limit;
	bool ovs_enabled;

	ovs_enabled = zebra_ovs_is_enabled();
	dplane_ctx_list_init(&work_list);

	limit = dplane_provider_get_work_limit(prov);

	for (counter = 0; counter < limit; counter++) {
		ctx = dplane_provider_dequeue_in_ctx(prov);
		if (!ctx)
			break;
		dplane_ctx_list_add_tail(&work_list, ctx);
	}

	if (ovs_enabled && counter > 0)
		zebra_ovs_update_multi(&work_list);

	while ((ctx = dplane_ctx_list_pop(&work_list)) != NULL) {
		if (ovs_enabled)
			dplane_ctx_set_skip_kernel(ctx);
		dplane_provider_enqueue_out_ctx(prov, ctx);
	}

	if (counter >= limit)
		dplane_provider_work_ready();

	return 0;
}

int zebra_ovs_dplane_register(void)
{
	return dplane_provider_register("OVS", DPLANE_PRIO_PRE_KERNEL,
					DPLANE_PROV_FLAGS_DEFAULT, NULL,
					ovs_dplane_process_func, NULL, NULL,
					NULL);
}
