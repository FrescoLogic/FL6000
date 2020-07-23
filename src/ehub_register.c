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

#include <linux/types.h>
#include <linux/usb.h>
#include <linux/workqueue.h>
#include <linux/slab.h>

#include "ehub_defines.h"

#include "ehub_device_context.h"
#include "ehub_register.h"
#include "ehub_utility.h"
#include "ehub_usb.h"

int
REGISTER_Transfer(
	PDEVICE_CONTEXT DeviceContext,
	int ReadWrite,
	u32 Offset,
	u32* Data
	)
{
	int status;
	u8 bRequest;
	u8 bRequestType;
	u16 wValue;
	u16 wIndex;
	char *dataBuffer;
	unsigned int usbPipe;

	dataBuffer = kzalloc( NUMBER_OF_BYTES_IN_REGISTER_DATA, GFP_ATOMIC );

	if ( ReadWrite == REGISTER_READ )
	{
		// Read
		//
		bRequest = CONTROL_ENDPOINT_I2C_CMD_READ;
		bRequestType = USB_DIR_IN | USB_TYPE_VENDOR;

		memset( dataBuffer, 0, NUMBER_OF_BYTES_IN_REGISTER_DATA);

		usbPipe = usb_rcvctrlpipe( DeviceContext->UsbContext.UsbDevice,
								   EHUB_ENDPOINT_NUMBER_CONTROL );
	}
	else
	{
		// Write
		//
		bRequest = CONTROL_ENDPOINT_I2C_CMD_WRITE;
		bRequestType = USB_DIR_OUT | USB_TYPE_VENDOR;

		memcpy( dataBuffer, Data, NUMBER_OF_BYTES_IN_REGISTER_DATA );

		usbPipe = usb_sndctrlpipe( DeviceContext->UsbContext.UsbDevice,
								   EHUB_ENDPOINT_NUMBER_CONTROL );
	}

	wValue = 0;
	wIndex = ( u16 )Offset;

	status = usb_control_msg( DeviceContext->UsbContext.UsbDevice,
							  usbPipe,
							  bRequest,
							  bRequestType,
							  wValue,
							  wIndex,
							  dataBuffer,
							  NUMBER_OF_BYTES_IN_REGISTER_DATA,
							  HZ );
	if ( status > 0 )
	{
		if ( ReadWrite == REGISTER_READ )
		{
			memcpy( Data, dataBuffer, NUMBER_OF_BYTES_IN_REGISTER_DATA );
		}
	}

	if ( NULL != dataBuffer )
		kfree( dataBuffer );

	return status;
}

inline int
REGISTER_Write(
	PDEVICE_CONTEXT DeviceContext,
	u32 Offset,
	u32* Data
	)
{
	int status;

	status = REGISTER_Transfer( DeviceContext,
								REGISTER_WRITE,
								Offset,
								Data );

	return status;
}

inline int
REGISTER_Read(
	PDEVICE_CONTEXT DeviceContext,
	u32 Offset,
	u32* Data
	)
{
	int status;

	*Data = 0;

	status = REGISTER_Transfer( DeviceContext,
								REGISTER_READ,
								Offset,
								Data );

	return status;
}

void
REGISTER_SetBit(
	PDEVICE_CONTEXT DeviceContext,
	u32 Offset,
	u32 BitOffset
	)
{
	int status;
	u32 data;

	status = REGISTER_Read( DeviceContext,
							Offset,
							&data );
	if ( ! status )
	{
		goto Exit;
	}

	data |= ( 1 << BitOffset );

	status = REGISTER_Write( DeviceContext,
							 Offset,
							 &data );
	if ( ! status )
	{
		goto Exit;
	}

Exit:
	;
}

void
REGISTER_ClearBit(
	PDEVICE_CONTEXT DeviceContext,
	u32 Offset,
	u32 BitOffset
	)
{
	int status;
	u32 data;

	status = REGISTER_Read( DeviceContext,
							Offset,
							&data );
	if ( ! status )
	{
		goto Exit;
	}

	data &= ~( 1 << BitOffset );

	status = REGISTER_Write( DeviceContext,
							 Offset,
							 &data );
	if ( ! status )
	{
		goto Exit;
	}

Exit:
	;
}
