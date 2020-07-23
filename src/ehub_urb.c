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

#include <linux/slab.h>
#include <linux/semaphore.h>

#include "ehub_defines.h"

#include "ehub_device_context.h"
#include "ehub_embedded_register.h"
#include "ehub_urb.h"
#include "ehub_utility.h"
#include "ehub_notification.h"
#include "ehub_work_item.h"
#include "xhci.h"

PURB_CONTEXT
URB_Create(
	PDEVICE_CONTEXT DeviceContext,
	struct usb_device* UsbDevice,
	unsigned int UsbPipe,
	int DataBufferLength,
	void(*UrbCompletionRoutine)(struct urb*),
	void *UrbCompletionContext,
	gfp_t flags
)
{
	PURB_CONTEXT urbContext;
	struct urb *urb;
	void *context;

	//FUNCTION_ENTRY;

	if (( NULL == DeviceContext->InterfaceBackup ) ||
		(( DeviceContext->InterfaceBackup->condition != USB_INTERFACE_BOUND ) &&
		 ( DeviceContext->InterfaceBackup->condition != USB_INTERFACE_BINDING ) )) {
		dev_err(dev_ctx_to_dev(DeviceContext), "ERROR Interface not bound condition=%d\n", DeviceContext->InterfaceBackup->condition );
		urbContext = NULL;
		goto Exit;
	}

	urbContext = kzalloc( sizeof( URB_CONTEXT ), flags );
	if ( NULL == urbContext )
	{
		DeviceContext->ErrorFlags.UrbContextAllocationError = 1;
		dev_err(dev_ctx_to_dev(DeviceContext), "ERROR Allocate urb context fail" );
		goto Exit;
	}

	if (UrbCompletionContext) {
		context = UrbCompletionContext;
	} else {
		context = urbContext;
	}

	urbContext->DeviceContextPvoid = ( void* )DeviceContext;
	urbContext->Dev = UsbDevice;
	urbContext->DataBufferLength = DataBufferLength;

	urbContext->DataBuffer = usb_alloc_coherent(UsbDevice,
												urbContext->DataBufferLength,
												flags,
												&urbContext->DataBufferDma);
	if (NULL == urbContext->DataBuffer) {
		DeviceContext->ErrorFlags.UsbAllocateCoherentError = 1;
		dev_err(dev_ctx_to_dev(DeviceContext), "ERROR usb_alloc_coherent error!");
		kfree(urbContext);
		goto Exit;
	}

	memset(urbContext->DataBuffer, 0, urbContext->DataBufferLength);

	urb = usb_alloc_urb(0, flags);
	if (NULL == urb) {
		DeviceContext->ErrorFlags.UsbAllocateUrbError = 1;
		dev_err(dev_ctx_to_dev(DeviceContext), "ERROR usb_alloc_urb error!");
		usb_free_coherent(UsbDevice,
						  urbContext->DataBufferLength,
						  urbContext->DataBuffer,
						  urbContext->DataBufferDma);
		kfree(urbContext);
		goto Exit;
	}

	if (usb_pipeint(UsbPipe)) {
		usb_fill_int_urb(urb,
						 UsbDevice,
						 UsbPipe,
						 urbContext->DataBuffer,
						 urbContext->DataBufferLength,
						 UrbCompletionRoutine,
						 context,
						 1 /* bInterval */);
	} else {
		usb_fill_bulk_urb(urb,
						  UsbDevice,
						  UsbPipe,
						  urbContext->DataBuffer,
						  urbContext->DataBufferLength,
						  UrbCompletionRoutine,
						  context);
	}

	urb->transfer_dma = urbContext->DataBufferDma;
	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	urbContext->Urb = urb;

	urbContext->Status = URB_STATUS_FREE;

	INIT_LIST_HEAD(&urbContext->list);

Exit:

	//FUNCTION_LEAVE;

	return urbContext;
}

PURB_CONTEXT
URB_CreateIsoch(
	PDEVICE_CONTEXT DeviceContext,
	struct usb_device* UsbDevice,
	unsigned int UsbPipe,
	int DataBufferLength,
	void(*UrbCompletionRoutine)(struct urb*),
	gfp_t flags
)
{
	PURB_CONTEXT urbContext;
	struct urb *urb;
	int pkt_idx;
	unsigned int offset;

	//FUNCTION_ENTRY;

	if ((NULL == DeviceContext->InterfaceBackup) ||
		((DeviceContext->InterfaceBackup->condition != USB_INTERFACE_BOUND) &&
		(DeviceContext->InterfaceBackup->condition != USB_INTERFACE_BINDING))) {
		dev_err(dev_ctx_to_dev(DeviceContext), "ERROR Interface not bound condition=%d\n", DeviceContext->InterfaceBackup->condition);
		urbContext = NULL;
		goto Exit;
	}

	urbContext = kzalloc(sizeof(URB_CONTEXT), flags);
	if (NULL == urbContext) {
		DeviceContext->ErrorFlags.UrbContextAllocationError = 1;
		dev_err(dev_ctx_to_dev(DeviceContext), "ERROR Allocate urb context fail");
		goto Exit;
	}

	urbContext->DeviceContextPvoid = ( void* )DeviceContext;
	urbContext->Dev = UsbDevice;
	urbContext->DataBufferLength = DataBufferLength * NUMBER_OF_MESSAGE_ISOCH_PKT;

	urbContext->DataBuffer = usb_alloc_coherent(UsbDevice,
												urbContext->DataBufferLength,
												flags,
												&urbContext->DataBufferDma);
	if (NULL == urbContext->DataBuffer) {
		DeviceContext->ErrorFlags.UsbAllocateCoherentError = 1;
		dev_err(dev_ctx_to_dev(DeviceContext), "ERROR usb_alloc_coherent error!");
		kfree(urbContext);
		goto Exit;
	}

	memset(urbContext->DataBuffer, 0, urbContext->DataBufferLength);

	urb = usb_alloc_urb(NUMBER_OF_MESSAGE_ISOCH_PKT,
						flags);
	if (NULL == urb) {
		DeviceContext->ErrorFlags.UsbAllocateUrbError = 1;
		dev_err(dev_ctx_to_dev(DeviceContext), "ERROR usb_alloc_urb error!");
		usb_free_coherent(UsbDevice,
						  urbContext->DataBufferLength,
						  urbContext->DataBuffer,
						  urbContext->DataBufferDma);

		kfree(urbContext);
		goto Exit;
	}

	urb->dev = UsbDevice;
	urb->pipe = UsbPipe;
	urb->transfer_buffer = urbContext->DataBuffer;
	urb->transfer_buffer_length = urbContext->DataBufferLength;
	urb->transfer_dma = urbContext->DataBufferDma;
	urb->complete = UrbCompletionRoutine;
	urb->context = urbContext;
	urb->interval = 1;
	urb->number_of_packets = NUMBER_OF_MESSAGE_ISOCH_PKT;

	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP | URB_ISO_ASAP | URB_DIR_IN;

	offset = 0;
	for (pkt_idx = 0; pkt_idx < urb->number_of_packets; pkt_idx++) {
		urb->iso_frame_desc[pkt_idx].offset = DataBufferLength * pkt_idx;
		urb->iso_frame_desc[pkt_idx].length = DataBufferLength;
	}

	urbContext->Urb = urb;

	urbContext->Status = URB_STATUS_FREE;

	INIT_LIST_HEAD(&urbContext->list);

Exit:

	//FUNCTION_LEAVE;

	return urbContext;
}

void
URB_Destroy(
	PURB_CONTEXT UrbContext
)
{
	PDEVICE_CONTEXT deviceContext = NULL;

	//FUNCTION_ENTRY;

	if (NULL == UrbContext) {
		printk(KERN_DEBUG "URB_Destoy: UrbContext is null!\n");
		goto Exit;
	}

	if (NULL == UrbContext->Urb) {
		printk(KERN_DEBUG "URB_Destoy: UrbContext->Urb is null!\n");
	} else {
		usb_free_urb(UrbContext->Urb);
	}

	if (NULL != UrbContext->DataBuffer) {
		/* Use DataBufferLength because Urb->transfer_buffer_length may have been changed. */
		usb_free_coherent(UrbContext->Dev, UrbContext->DataBufferLength,
						  UrbContext->DataBuffer, UrbContext->DataBufferDma);
		UrbContext->DataBuffer = NULL;
		UrbContext->DataBufferLength = 0;
		UrbContext->DataBufferDma = 0;
	}
	list_del_init(&UrbContext->list);

	if (NULL != UrbContext->DeviceContextPvoid) {
		deviceContext = ( PDEVICE_CONTEXT )UrbContext->DeviceContextPvoid;
	}

	kfree(UrbContext);

Exit:
	;
	//FUNCTION_LEAVE;
}

int
URB_Submit(
	PURB_CONTEXT UrbContext
	)
{
	int status;

	//FUNCTION_ENTRY;

	NOTIFICATION_Reset( &UrbContext->Event );

	UrbContext->Status = URB_STATUS_SUBMITTED;

	status = usb_submit_urb( UrbContext->Urb,
							 GFP_ATOMIC );
	if (status < 0)
	{
		dev_err(dev_ctx_to_dev(UrbContext->DeviceContextPvoid), "ERROR usb_submit_urb fail from %ps: %d\n", __builtin_return_address(0), status );
		UrbContext->Status = URB_STATUS_SUBMIT_ERROR;
	}

	//FUNCTION_LEAVE;

	return status;
}

/* TODO: EHUB pass URB to message handler to avoid this extra memory copy. */
void
URB_CompletionRoutine_GetMessage(
	struct urb *Urb
	)
{
	PDEVICE_CONTEXT deviceContext;
	PURB_CONTEXT urbContext;
	u8* xfer_buf;
	int xfer_buf_len;
	int status;

	//FUNCTION_ENTRY;

	if (unlikely(!Urb))
		goto Exit;

	urbContext = ( PURB_CONTEXT )Urb->context;
	if (unlikely(!urbContext))
		goto Exit;

	deviceContext = ( PDEVICE_CONTEXT )urbContext->DeviceContextPvoid;
	if (unlikely(!deviceContext))
		goto Exit;

	status = DEVICECONTEXT_ErrorCheck( deviceContext );
	if (unlikely(status < 0))
	{
		goto Exit;
	}

	xfer_buf = Urb->transfer_buffer;
	xfer_buf_len = Urb->actual_length;

	urbContext->Status = URB_STATUS_COMPLETE;

	if ( NULL != xfer_buf )
	{
		if ( xfer_buf_len > 0 )
		{
			PWORK_ITEM_CONTEXT workItemContext;
			int pkt_idx;

			workItemContext = WORK_ITEM_Create( deviceContext,
												WORK_ITEM_Process_MessageHandle,
												xfer_buf_len );
			ASSERT( NULL != workItemContext );

			if (!Urb->number_of_packets) {
				memcpy(workItemContext->DataBuffer,
					   xfer_buf,
					   xfer_buf_len);
			} else {
				/* Combine isoch data so it can be processed as one big transfer. */
				u8* data_buf = workItemContext->DataBuffer;
				for (pkt_idx = 0; pkt_idx < Urb->number_of_packets; pkt_idx++)
					if (!Urb->iso_frame_desc[pkt_idx].status) {
						memcpy(data_buf,
							Urb->transfer_buffer + Urb->iso_frame_desc[pkt_idx].offset,
							   Urb->iso_frame_desc[pkt_idx].actual_length);
						data_buf += Urb->iso_frame_desc[pkt_idx].actual_length;
					}
			}

			WORK_ITEM_Submit( workItemContext );
		}
		else
		{
			/* Ok for isoch Urbs to have no data. */
			if(unlikely((-ESHUTDOWN != Urb->status) && !Urb->number_of_packets))
				dev_err(dev_ctx_to_dev(deviceContext), "ERROR xfer_buf_len = 0 \n");
		}
	}
	else
	{
		dev_err(dev_ctx_to_dev(deviceContext), "ERROR Urb DataBufferError!" );
		goto Exit;
	}

	if (likely(Urb->status == 0 ))
	{
		status = URB_Submit( urbContext );
		if (unlikely(status < 0 ))
		{
			goto Exit;
		}
	}
	else
	{
		if ( -ESHUTDOWN != Urb->status )
			dev_err(dev_ctx_to_dev(deviceContext), "ERROR Urb status code is not zero : %d \n", Urb->status);
	}

Exit:
	;
	//FUNCTION_LEAVE;
}

void
URB_CompletionRoutine_Simple(
	struct urb *Urb
	)
{
	PDEVICE_CONTEXT deviceContext;
	PURB_CONTEXT urbContext;

	//FUNCTION_ENTRY;

	if (!Urb)
		return;

	if (Urb->status == -ESHUTDOWN)
		return;

	urbContext = ( PURB_CONTEXT )Urb->context;
	if (!urbContext)
		return;

	deviceContext = ( PDEVICE_CONTEXT )urbContext->DeviceContextPvoid;
	if (!deviceContext)
		return;

	if( unlikely( DEVICECONTEXT_ErrorCheck(deviceContext) < 0 ))
		return;

	dev_dbg(dev_ctx_to_dev(deviceContext), "urbContext 0x%08LX\n", ( u64 )urbContext);

	urbContext->ActualLength = Urb->actual_length;
	urbContext->UrbCompletionStatus = Urb->status;
	if (Urb->status < 0)
		dev_err(dev_ctx_to_dev(deviceContext), "Cmp_Simple: Error status %d\n", Urb->status);

	urbContext->Status = URB_STATUS_COMPLETE;

	NOTIFICATION_Notify( deviceContext,
						 &urbContext->Event );

	//FUNCTION_LEAVE;
}

void
URB_CompletionRoutine_Doorbell(
	struct urb *Urb
)
{
	PDEVICE_CONTEXT deviceContext;
	PURB_CONTEXT urbContext;

	//FUNCTION_ENTRY;

	if (!Urb)
		return;

	if (Urb->status == -ESHUTDOWN)
		return;

	urbContext = ( PURB_CONTEXT )Urb->context;
	if (!urbContext)
		return;

	deviceContext = ( PDEVICE_CONTEXT )urbContext->DeviceContextPvoid;
	if (!deviceContext)
		return;

	if (unlikely(DEVICECONTEXT_ErrorCheck(deviceContext) < 0))
		return;

	dev_dbg(dev_ctx_to_dev(deviceContext), "urbContext 0x%08LX\n", ( u64 )urbContext);

	urbContext->ActualLength = Urb->actual_length;
	urbContext->UrbCompletionStatus = Urb->status;
	if (Urb->status < 0)
		dev_err(dev_ctx_to_dev(deviceContext), "Cmp_DB: Error status %d\n", Urb->status);

	urbContext->Status = URB_STATUS_COMPLETE;

	NOTIFICATION_Notify(deviceContext,
						&urbContext->Event);

	list_move_tail(&urbContext->list, &deviceContext->doorbell_list_free);

	//FUNCTION_LEAVE;
}
