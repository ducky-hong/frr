// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Zebra userspace mock provider header
 * Copyright (C) 2025 Free Range Routing
 */

#ifndef _ZEBRA_USRSPACE_MOCK_H
#define _ZEBRA_USRSPACE_MOCK_H

#include "zebra/zebra_usrspace_provider.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize mock provider */
void zebra_usrspace_mock_init(void);

/* Direct event injection for testing */
int zebra_usrspace_mock_inject(struct usrspace_event *event);

/* Initialize VTY commands */
void zebra_usrspace_mock_vty_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _ZEBRA_USRSPACE_MOCK_H */
