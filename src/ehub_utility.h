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

#ifndef EHUB_UTILITY_H
#define EHUB_UTILITY_H

#define KDEBUG_MESSAGE_PREFIX        "[EHUB]"

#define KOUT( Format, Arguments...) \
	do { printk(KERN_ERR "%s" Format , KDEBUG_MESSAGE_PREFIX , ## Arguments); } while (0)

#ifdef USE_FUNCTION_ENTRY
#define FUNCTION_ENTRY KOUT( "%s >>> \n", __FUNCTION__ )
#define FUNCTION_ENTRY_WITH_VALUE( _VALUE_ ) KOUT( "%s : 0x%x >>> \n", __FUNCTION__, _VALUE_ )
#else
#define FUNCTION_ENTRY {};
#define FUNCTION_ENTRY_WITH_VALUE( _VALUE_ ) {};
#endif

#ifdef USE_FUNCTION_LEAVE
#define FUNCTION_LEAVE KOUT( "%s <<< \n", __FUNCTION__ )
#define FUNCTION_LEAVE_WITH_VALUE( _VALUE_ ) KOUT( "%s : 0x%x <<< \n", __FUNCTION__, _VALUE_ )
#else
#define FUNCTION_LEAVE {};
#define FUNCTION_LEAVE_WITH_VALUE( _VALUE_ ) {};
#endif

#ifdef USE_KGDB_BP
	#define KGDB_BP kgdb_breakpoint();
#else
	#define KGDB_BP {};
#endif

#ifdef USE_KERNEL_PANIC
#define KERNEL_PANIC( _KERNEL_PANIC_MESSAGE_ ) \
	do { \
		 printk(KERN_ERR _KERNEL_PANIC_MESSAGE_ ); \
		 KGDB_BP \
		 panic( _KERNEL_PANIC_MESSAGE_); } while (0)
#else
#define KERNEL_PANIC( _KERNEL_PANIC_MESSAGE_ ) \
	do { \
		printk(KERN_ERR _KERNEL_PANIC_MESSAGE_ ); \
		KGDB_BP \
	} while (0)
#endif

/* TODO EHUB add an error message here for when gdb isn't available */
#define ASSERT(x) \
	do { if (!(x)) KGDB_BP; } while (0)

#define ehub_xhci_err(xhci, fmt, args...) \
	dev_err(xhci_to_hcd(xhci)->self.controller , fmt , ## args)

#endif
