// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Userspace Notification Provider Header
 *
 * Copyright (C) 2025 FRR Community
 */

#ifndef _NOTIFY_USERSPACE_H
#define _NOTIFY_USERSPACE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize userspace notification provider */
int notify_userspace_init(const char *socket_path);

/* Cleanup userspace provider */
void notify_userspace_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* _NOTIFY_USERSPACE_H */
