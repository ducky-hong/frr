// SPDX-License-Identifier: GPL-2.0-or-later
/* lib/version.h.  Generated from version.h.in by configure.
 *
 * Quagga version
 * Copyright (C) 1997, 1999 Kunihiro Ishiguro
 */

#ifndef _ZEBRA_VERSION_H
#define _ZEBRA_VERSION_H

#ifdef GIT_VERSION
#include "lib/gitversion.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GIT_SUFFIX
#define GIT_SUFFIX ""
#endif
#ifndef GIT_INFO
#define GIT_INFO ""
#endif

#define FRR_PAM_NAME    "frr"
#define FRR_SMUX_NAME   "frr"
#define FRR_PTM_NAME    "frr"

#define FRR_FULL_NAME   "FRRouting"
#define FRR_VERSION     "10.6-dev" GIT_SUFFIX
#define FRR_VER_SHORT   "10.6-dev"
#define FRR_BUG_ADDRESS "https://github.com/frrouting/frr/issues"
#define FRR_COPYRIGHT   "Copyright 1996-2005 Kunihiro Ishiguro, et al."
#define FRR_CONFIG_ARGS "'--enable-clippy-only' 'PKG_CONFIG_PATH=/root/.local/lib/pkgconfig'"

#define FRR_DEFAULT_MOTD \
	"\n" \
	"Hello, this is " FRR_FULL_NAME " (version " FRR_VERSION ").\n" \
	FRR_COPYRIGHT "\n" \
	GIT_INFO "\n"

pid_t pid_output (const char *);

#ifdef __cplusplus
}
#endif

#endif /* _ZEBRA_VERSION_H */
