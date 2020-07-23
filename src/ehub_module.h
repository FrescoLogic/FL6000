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

#ifndef EHUB_MODULE_H
#define EHUB_MODULE_H

#define STRING_MODULE_LICENSE       "GPL"
#define STRING_MODULE_DESCRIPTION   "Fresco Logic eHub device driver - Version 0.6.0.0"
#define STRING_MODULE_AUTHOR        "Fresco Logic"

int
ehub_xhci_doorbell_expand(
	PDEVICE_CONTEXT DeviceContext,
	ulong entries,
	gfp_t flags
);
#endif
