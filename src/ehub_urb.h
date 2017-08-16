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

#ifndef EHUB_URB_H
#define EHUB_URB_H

#define URB_STATUS_NONE         0
#define URB_STATUS_FREE         1
#define URB_STATUS_SUBMITTED    2
#define URB_STATUS_COMPLETE     3
#define URB_STATUS_SUBMIT_ERROR 4

PURB_CONTEXT
URB_Create(
	PDEVICE_CONTEXT DeviceContext,
	struct usb_device* UsbDevice,
	unsigned int UsbPipe,
	int DataBufferLength,
	void ( *UrbCompletionRoutine )( struct urb* ),
	void *UrbCompletionContext,
	gfp_t flags
	);

PURB_CONTEXT
URB_CreateIsoch(
	PDEVICE_CONTEXT DeviceContext,
	struct usb_device* UsbDevice,
	unsigned int UsbPipe,
	int DataBufferLength,
	void(*UrbCompletionRoutine)(struct urb*),
	gfp_t flags
);

void
URB_Destroy(
	PURB_CONTEXT UrbContext
	);

int
URB_Submit(
	PURB_CONTEXT UrbContext
	);

void
URB_CompletionRoutine_GetMessage(
	struct urb *Urb
	);

void
URB_CompletionRoutine_Simple(
	struct urb *Urb
	);

void
URB_CompletionRoutine_Doorbell(
	struct urb *Urb
	);

#endif
