/*
 * Fresco Logic FL6000 F-One Controller Driver
 *
 * Copyright (C) 2014-2017 Fresco Logic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef EHUB_DEFINES_H
#define EHUB_DEFINES_H

/* Define manually since it can cause problems. */
#define NO_EHUB_TRACING

#ifdef EHUB_DEBUG_ENABLE
#include <linux/kgdb.h>
//#define USE_FUNCTION_ENTRY
//#define USE_FUNCTION_LEAVE
#ifndef CONFIG_ANDROID
#define NO_USE_KGDB_BP
#define NO_USE_KERNEL_PANIC
#endif /* !CONFIG_ANDROID */
#else /* ! EHUB_DEBUG_ENABLE */
#define NO_USE_FUNCTION_ENTRY
#define NO_USE_FUNCTION_LEAVE
#define NO_USE_KGDB_BP
#define NO_USE_KERNEL_PANIC
#endif /* ! EHUB_DEBUG_ENABLE */

// Feature Control.
//
#define NO_USE_XHCI_EHUB_SPIN_LOCK
#define USE_TRB_CACHE_MODE
#define NO_USE_DELAYED_CACHE_MODE
#define EHUB_ISOCH_ENABLE
#define EHUB_ISOCH_DATA_CACHE_ENABLE

#endif /* EHUB_DEFINES_H */
