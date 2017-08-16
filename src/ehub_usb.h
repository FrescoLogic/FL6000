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

#ifndef EHUB_USB_H
#define EHUB_USB_H

#define EHUB_DEVICE_VENDOR                              ( 0x1D5C )
#define EHUB_DEVICE_PRODUCT                             ( 0x6000 )

#define EHUB_CONFIGURATION_INDEX_0                      ( 0 )
#define EHUB_CONFIGURATION_INDEX_1                      ( 1 )

#define EHUB_INTERFACE_CLASS                            ( 0xFF )
#define EHUB_INTERFACE_SUBCLASS                         ( 0x05 )

#define EHUB_INTERFACE_NUMBER_BULK                      ( 0 )
#define EHUB_INTERFACE_NUMBER_ISOCH                     ( 1 )
#define EHUB_INTERFACE_NUMBER_INTERRUPT                 ( 2 )

#define EHUB_INTERFACE_ALTERNATE_SETTING_BULK           ( 0 )
#define EHUB_INTERFACE_ALTERNATE_SETTING_ISOCH          ( 1 )
#define EHUB_INTERFACE_ALTERNATE_SETTING_INTERRUPT      ( 2 )

#define EHUB_INTERFACE_PROTOCOL_BULK                    ( 1 )
#define EHUB_INTERFACE_PROTOCOL_ISOCH                   ( 2 )
#define EHUB_INTERFACE_PROTOCOL_INTERRUPT               ( 3 )

#define EHUB_ENDPOINT_NUMBER_CONTROL                    ( 0 )
#define EHUB_ENDPOINT_NUMBER_ISOCH                      ( 1 )
#define EHUB_ENDPOINT_NUMBER_INTERRUPT                  ( 2 )
#define EHUB_ENDPOINT_NUMBER_BULK                       ( 3 )

#define MAX_USB_DESCRIPTOR_SIZE ( 512 )

#define BULK_TRANSFER_TIMEOUT_WAIT_FOREVER  ( 0 )
#define BULK_TRANSFER_TIMEOUT_IM_MS         ( 500 )

int
USB_InterfaceCreateBulk(
	PDEVICE_CONTEXT DeviceContext
	);

int
USB_InterfaceCreateIsoch(
	PDEVICE_CONTEXT DeviceContext
	);

int
USB_InterfaceCreateInterrupt(
	PDEVICE_CONTEXT DeviceContext
	);

int
USB_InterfaceDestroyBulk(
	PDEVICE_CONTEXT DeviceContext
	);

int
USB_InterfaceDestroyIsoch(
	PDEVICE_CONTEXT DeviceContext
	);

int
USB_InterfaceDestroyInterrupt(
	PDEVICE_CONTEXT DeviceContext
	);

int
USB_InterfaceSuspendBulk(
	PDEVICE_CONTEXT DeviceContext
	);

int
USB_InterfaceSuspendIsoch(
	PDEVICE_CONTEXT DeviceContext
	);

int
USB_InterfaceSuspendInterrupt(
	PDEVICE_CONTEXT DeviceContext
	);

int
USB_InterfaceResumeBulk(
	PDEVICE_CONTEXT DeviceContext
	);

int
USB_InterfaceResumeIsoch(
	PDEVICE_CONTEXT DeviceContext
	);

int
USB_InterfaceResumeInterrupt(
	PDEVICE_CONTEXT DeviceContext
	);

#endif
