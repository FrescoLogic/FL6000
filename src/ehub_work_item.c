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
#include <linux/workqueue.h>
#include <linux/usb.h>
#include <linux/slab.h>

#include "xhci.h"

#include "ehub_defines.h"

#include "ehub_device_context.h"
#include "ehub_utility.h"
#include "ehub_message.h"
#include "ehub_notification.h"
#include "ehub_urb.h"
#include "ehub_work_item.h"

/* TODO: EHUB Allocate the work item context on connect and
 * reuse it instead of allocating and freeing repeatedly. */
PWORK_ITEM_CONTEXT
WORK_ITEM_Create(
	PDEVICE_CONTEXT DeviceContext,
	void ( *WorkItemProcess )( struct work_struct * ),
	int DataBufferLength
	)
{
	PWORK_ITEM_CONTEXT workItemContext;

	workItemContext = kzalloc( sizeof( WORK_ITEM_CONTEXT ), GFP_ATOMIC );
	ASSERT( NULL != workItemContext );

	workItemContext->DeviceContextPvoid = ( void* )DeviceContext;
	workItemContext->DataBufferLength = DataBufferLength;
	workItemContext->WorkItemProcess = WorkItemProcess;

	workItemContext->DataBuffer = kzalloc( DataBufferLength, GFP_ATOMIC );
	ASSERT( NULL != workItemContext->DataBuffer );

	return workItemContext;
}

void
WORK_ITEM_Destroy(
	PWORK_ITEM_CONTEXT WorkItemContext
	)
{
	PDEVICE_CONTEXT deviceContext;
	unsigned long flags;

	deviceContext = ( PDEVICE_CONTEXT )WorkItemContext->DeviceContextPvoid;
	if (!deviceContext) {
		printk(KERN_ERR "ERROR WORK_ITEM_Destroy deviceContext null\n");
		return;
	}

	spin_lock_irqsave( &deviceContext->SpinLockWorkItemQueue , flags );

	if ( NULL != WorkItemContext->DataBuffer )
		kfree( WorkItemContext->DataBuffer );

	WorkItemContext->DataBuffer = NULL;
	WorkItemContext->DataBufferLength = 0;
	WorkItemContext->WorkItemProcess = NULL;
	WorkItemContext->DeviceContextPvoid = NULL;

	list_del_init( &WorkItemContext->list );
	deviceContext->NumberOfWorkItemInProcessingQueue--;

	//dev_dbg(dev_ctx_to_dev(deviceContext), "NumberOfWorkItemInProcessingQueue : %d \n", deviceContext->NumberOfWorkItemInProcessingQueue );

	if ( NULL != WorkItemContext)
		kfree(WorkItemContext);

	spin_unlock_irqrestore( &deviceContext->SpinLockWorkItemQueue , flags );
}

void
WORK_ITEM_Submit(
	PWORK_ITEM_CONTEXT WorkItemContext
)
{
	PDEVICE_CONTEXT deviceContext;
	unsigned long flags;

	deviceContext = ( PDEVICE_CONTEXT )WorkItemContext->DeviceContextPvoid;
	if (!deviceContext) {
		printk(KERN_ERR "ERROR %s deviceContext null\n", __FUNCTION__);
		return;
	}

	spin_lock_irqsave(&deviceContext->SpinLockWorkItemQueue, flags);

	INIT_DELAYED_WORK(&WorkItemContext->WorkItem,
					  WorkItemContext->WorkItemProcess);

	list_add_tail(&WorkItemContext->list,
				  &deviceContext->WorkItemProcessingQueue);

	deviceContext->NumberOfWorkItemInProcessingQueue++;

	queue_delayed_work(deviceContext->WorkItemQueue, &WorkItemContext->WorkItem, 0 );

	spin_unlock_irqrestore(&deviceContext->SpinLockWorkItemQueue, flags);
}

void
WORK_ITEM_Process_MessageHandle(
	struct work_struct* WorkItem
	)
{
	PWORK_ITEM_CONTEXT workItemContext;
	PDEVICE_CONTEXT deviceContext;

	workItemContext = container_of( WorkItem,
									WORK_ITEM_CONTEXT,
									WorkItem.work );

	deviceContext = ( PDEVICE_CONTEXT )workItemContext->DeviceContextPvoid;

	MESSAGE_Parsing( deviceContext,
					 workItemContext->DataBuffer,
					 workItemContext->DataBufferLength );

	WORK_ITEM_Destroy( workItemContext );
}

void
WORK_ITEM_Process_GenericBulkOutUrbSubmit(
	struct work_struct* WorkItem
	)
{
	int status;
	PWORK_ITEM_CONTEXT workItemContext;
	PDEVICE_CONTEXT deviceContext;
	PURB_CONTEXT urbContext;
	u8* dataBuffer;
	int dataBufferLength;

	FUNCTION_ENTRY;

	urbContext = NULL;
	dataBuffer = NULL;

	printk(KERN_DEBUG "WORK_ITEM_Process_GenericBulkOutUrbSubmit: WorkItem=0x%p", WorkItem );

	workItemContext = container_of( WorkItem,
									WORK_ITEM_CONTEXT,
									WorkItem.work );
	ASSERT( NULL != workItemContext );

	printk(KERN_DEBUG "WORK_ITEM_Process_GenericBulkOutUrbSubmit: workItemContext=0x%p", workItemContext );
	printk(KERN_DEBUG "WORK_ITEM_Process_GenericBulkOutUrbSubmit: From : %d \n", workItemContext->FromWhere );

	deviceContext = ( PDEVICE_CONTEXT )workItemContext->DeviceContextPvoid;
	ASSERT( NULL != deviceContext );

	status = DEVICECONTEXT_ErrorCheck( deviceContext );
	if (status < 0)
	{
		goto Exit;
	}

	dataBuffer = workItemContext->DataBuffer;
	dataBufferLength = workItemContext->DataBufferLength;

	urbContext = URB_Create(deviceContext,
							deviceContext->UsbContext.UsbDevice,
							deviceContext->UsbContext.UsbPipeBulkOut,
							dataBufferLength,
							URB_CompletionRoutine_Simple,
							NULL,
							GFP_KERNEL);
	if ( NULL == urbContext )
	{
		dev_err(dev_ctx_to_dev(deviceContext), "ERROR WORK_ITEM_Process_GenericBulkOutUrbSubmit: URB_Create fail!" );
		goto Exit;
	}

	memcpy(urbContext->Urb->transfer_buffer, dataBuffer, dataBufferLength);
	urbContext->Urb->transfer_buffer_length = dataBufferLength;

	status = URB_Submit( urbContext );
	if (status < 0)
	{
		dev_err(dev_ctx_to_dev(deviceContext), "ERROR WORK_ITEM_Process_GenericBulkOutUrbSubmit: URB_Submit fail! %d\n", status );
		goto Exit;
	}

	status = NOTIFICATION_Wait( deviceContext,
								&urbContext->Event,
								NOTIFICATION_EVENT_TIMEOUT );
	if (status < 0)
	{
		dev_err(dev_ctx_to_dev(deviceContext), "ERROR WORK_ITEM_Process_GenericBulkOutUrbSubmit: Wait fail! %d\n", status );
		goto Exit;
	}

Exit:

	URB_Destroy( urbContext );

	WORK_ITEM_Destroy( workItemContext );

	FUNCTION_LEAVE;
}

void
WORK_ITEM_Process_MessageHandleEmbeddedEmbeddedRegisterRead(
	struct work_struct* WorkItem
	)
{
	PWORK_ITEM_CONTEXT workItemContext;
	PDEVICE_CONTEXT deviceContext;
	u32* data;

	FUNCTION_ENTRY;

	workItemContext = container_of( WorkItem,
									WORK_ITEM_CONTEXT,
									WorkItem.work );

	deviceContext = ( PDEVICE_CONTEXT )workItemContext->DeviceContextPvoid;

	data = ( u32* )workItemContext->DataBuffer;

	deviceContext->DataEmbeddedRegisterRead = *data;

	NOTIFICATION_Notify( deviceContext,
						 &deviceContext->CompletionEventEmbeddedRegisterRead );

	WORK_ITEM_Destroy( workItemContext );

	FUNCTION_LEAVE;
}

void
WORK_ITEM_Process_MessageHandleEmbeddedEventTrb(
	struct work_struct* WorkItem
	)
{
	PWORK_ITEM_CONTEXT workItemContext;
	PDEVICE_CONTEXT deviceContext;
	struct xhci_generic_trb *eventTrb;

	FUNCTION_ENTRY;

	workItemContext = container_of( WorkItem,
									WORK_ITEM_CONTEXT,
									WorkItem.work );

	deviceContext = ( PDEVICE_CONTEXT )workItemContext->DeviceContextPvoid;

	eventTrb = ( struct xhci_generic_trb * )workItemContext->DataBuffer;

	ehub_xhci_handle_event( deviceContext->UsbContext.xhci_hcd, *eventTrb );

	WORK_ITEM_Destroy( workItemContext );

	FUNCTION_LEAVE;
}

