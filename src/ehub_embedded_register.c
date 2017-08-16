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

#include "ehub_defines.h"
#include "xhci.h"
#include "ehub_embedded_register.h"
#include "ehub_notification.h"
#include "ehub_urb.h"
#include "ehub_module.h"

int
EMBEDDED_REGISTER_Read(
	PDEVICE_CONTEXT DeviceContext,
	u32 Address,
	u32* Data
	)
{
	PURB_CONTEXT urbContext;
	PEMBEDDED_REGISTER_COMMAND embeddedRegisterCommand;
	int status;

	FUNCTION_ENTRY;

	might_sleep();
	if (down_interruptible(&DeviceContext->EmbeddedRegisterLock))
		ASSERT(false);

	urbContext = DeviceContext->UrbContextEmbeddedRegisterRead;

	if (!urbContext) {
		*Data = ~(0);
		status = -ESHUTDOWN;
		goto Exit;
	}

	embeddedRegisterCommand = ( PEMBEDDED_REGISTER_COMMAND )urbContext->DataBuffer;

	memset( embeddedRegisterCommand, 0, sizeof( EMBEDDED_REGISTER_COMMAND ) );

	embeddedRegisterCommand->Address = Address;
	embeddedRegisterCommand->ByteEnables = 0x0F;
	embeddedRegisterCommand->RegAccess = true;
	embeddedRegisterCommand->Read = true;
	embeddedRegisterCommand->Write = false;

	NOTIFICATION_Reset( &urbContext->Event );
	NOTIFICATION_Reset( &DeviceContext->CompletionEventEmbeddedRegisterRead );

	status = URB_Submit( urbContext );
	if (status < 0)
	{
		*Data = ~(0);
		dev_err(dev_ctx_to_dev(DeviceContext), "ERROR Read addr 0x%X URB_Submit error! %d\n", Address, status );
		goto Exit;
	}

	status = NOTIFICATION_Wait( DeviceContext,
								&urbContext->Event,
								NOTIFICATION_EVENT_TIMEOUT );
	if (status < 0)
	{
		*Data = ~(0);
		dev_err(dev_ctx_to_dev(DeviceContext), "ERROR Read addr 0x%X NOTIFICATION_Wait error! %d\n", Address, status );
		goto Exit;
	}

	status = NOTIFICATION_Wait( DeviceContext,
								&DeviceContext->CompletionEventEmbeddedRegisterRead,
								NOTIFICATION_EVENT_TIMEOUT );
	if (status < 0)
	{
		*Data = ~(0);
		dev_err(dev_ctx_to_dev(DeviceContext), "ERROR Read addr 0x%X NOTIFICATION_Wait Read error! %d\n", Address, status );
	}
	else
	{
		*Data = DeviceContext->DataEmbeddedRegisterRead;
		dev_dbg ( dev_ctx_to_dev ( DeviceContext ), "0x%08x = %08xh \n",
				   Address,
				   *Data );
	}

Exit:

	if (status < 0)
	{
		DeviceContext->ErrorFlags.EmbeddedRegisterError = 1;
	}

	up(&DeviceContext->EmbeddedRegisterLock);

	FUNCTION_LEAVE;

	return status;
}

int
EMBEDDED_REGISTER_Write(
	PDEVICE_CONTEXT DeviceContext,
	u32 Address,
	u32* Data
	)
{
	PURB_CONTEXT urbContext;
	PEMBEDDED_REGISTER_COMMAND embeddedRegisterCommand;
	PEMBEDDED_REGISTER_DATA_TRANSFER embedded_register_transfer;
	int status;

	FUNCTION_ENTRY;

	might_sleep();
	if (down_interruptible(&DeviceContext->EmbeddedRegisterLock))
		ASSERT(false);

	dev_dbg(dev_ctx_to_dev(DeviceContext),
			"WriteAddress : 0x%04x , WriteData : 0x%08x \n",
			Address,
			*Data);

	urbContext = DeviceContext->UrbContextEmbeddedRegisterWrite;

	embedded_register_transfer = ( PEMBEDDED_REGISTER_DATA_TRANSFER )urbContext->DataBuffer;

	embeddedRegisterCommand = &embedded_register_transfer->EmbeddedRegisterCommand;

	memset( embeddedRegisterCommand, 0, sizeof( EMBEDDED_REGISTER_COMMAND ) );

	embeddedRegisterCommand->Address = Address;
	embeddedRegisterCommand->ByteEnables = 0xFF;
	embeddedRegisterCommand->RegAccess = true;
	embeddedRegisterCommand->Read = false;
	embeddedRegisterCommand->Write = true;

	embedded_register_transfer->Data = *Data;

	status = URB_Submit( urbContext );
	if (status < 0)
	{
		*Data = ~(0);
		dev_err(dev_ctx_to_dev(DeviceContext), "ERROR Write addr 0x%X URB_Submit fail! %d\n", Address, status );
		goto Exit;
	}

	status = NOTIFICATION_Wait( DeviceContext,
								&urbContext->Event,
								NOTIFICATION_EVENT_TIMEOUT );
	if (status < 0)
	{
		*Data = ~(0);
		dev_err(dev_ctx_to_dev(DeviceContext), "ERROR Write addr 0x%X NOTIFICATION_Wait timeout! %d\n", Address, status );
		goto Exit;
	}

Exit:

	if (status < 0)
	{
		DeviceContext->ErrorFlags.EmbeddedRegisterError = 1;
	}

	up(&DeviceContext->EmbeddedRegisterLock);

	FUNCTION_LEAVE;

	return status;
}

int
EMBEDDED_REGISTER_Write_Doorbell(
	PDEVICE_CONTEXT DeviceContext,
	u32 Address,
	u32* Data
	)
{
	PURB_CONTEXT urbContext;
	PEMBEDDED_REGISTER_COMMAND embeddedRegisterCommand;
	PEMBEDDED_REGISTER_DATA_TRANSFER embedded_register_transfer;
	int status;
	unsigned long flags;
	int new_entries = 1;

	FUNCTION_ENTRY;

	dev_dbg(dev_ctx_to_dev(DeviceContext), "WriteAddress : 0x%08x , WriteData : 0x%08x \n",
			Address,
			*Data);

	spin_lock_irqsave( &DeviceContext->SpinLockEmbeddedDoorbellWrite, flags);

	if (list_empty(&DeviceContext->doorbell_list_free))
		new_entries = ehub_xhci_doorbell_expand(DeviceContext, 4, GFP_ATOMIC);

	if (new_entries < 1) {
		ASSERT(false);
		spin_unlock_irqrestore(&DeviceContext->SpinLockEmbeddedDoorbellWrite, flags);
		return -ENOBUFS;
	}

	urbContext = list_first_entry(&DeviceContext->doorbell_list_free,
								  URB_CONTEXT,
								  list);

	list_move_tail(&urbContext->list, &DeviceContext->doorbell_list_busy);
	spin_unlock_irqrestore( &DeviceContext->SpinLockEmbeddedDoorbellWrite, flags );

	embedded_register_transfer = ( PEMBEDDED_REGISTER_DATA_TRANSFER )urbContext->DataBuffer;

	embeddedRegisterCommand = &embedded_register_transfer->EmbeddedRegisterCommand;

	memset( embeddedRegisterCommand, 0, sizeof( EMBEDDED_REGISTER_COMMAND ) );

	embeddedRegisterCommand->Address = Address;
	embeddedRegisterCommand->ByteEnables = 0xFF;
	embeddedRegisterCommand->RegAccess = true;
	embeddedRegisterCommand->Read = false;
	embeddedRegisterCommand->Write = true;

	embedded_register_transfer->Data = *Data;

	status = URB_Submit( urbContext );
	if (status < 0)
	{
		*Data = ~(0);
		dev_err(dev_ctx_to_dev(DeviceContext), "ERROR URB_Submit fail! %d\n", status);
		goto Exit;
	}

Exit:

	if (status < 0)
	{
		DeviceContext->ErrorFlags.EmbeddedRegisterError = 1;
	}

	FUNCTION_LEAVE;

	return status;
}

