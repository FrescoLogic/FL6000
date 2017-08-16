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

#include "ehub_public.h"

PDEVICE_CONTEXT
DEVICECONTEXT_Create( struct device *dev)
{
	PDEVICE_CONTEXT deviceContext;

	FUNCTION_ENTRY;

	deviceContext = kzalloc( sizeof( DEVICE_CONTEXT ), GFP_ATOMIC );
	if (!deviceContext)
		return deviceContext;

	INIT_LIST_HEAD(&deviceContext->doorbell_list_free);
	INIT_LIST_HEAD(&deviceContext->doorbell_list_busy);

	INIT_LIST_HEAD( &deviceContext->WorkItemPendingQueue );
	INIT_LIST_HEAD( &deviceContext->WorkItemProcessingQueue );

//  deviceContext->WorkItemQueue = create_singlethread_workqueue("ehub_WorkItemQueue");
	deviceContext->WorkItemQueue = alloc_ordered_workqueue("ehub_WorkItemQueue", WQ_HIGHPRI | WQ_MEM_RECLAIM);
	if (!deviceContext->WorkItemQueue) {
		dev_dbg(dev, "ERROR failed to create WorkItemQueue\n");
		return NULL;
	}

	spin_lock_init( &deviceContext->SpinLockWorkItemQueue );
	spin_lock_init( &deviceContext->SpinLockEmbeddedDoorbellWrite );
	deviceContext->NumberOfWorkItemInProcessingQueue = 0;

	mutex_init(&deviceContext->MessageHandleEmbeddedMemoryReadCompletionLock);
	sema_init(&deviceContext->EmbeddedRegisterLock, 1);

	FUNCTION_LEAVE;

	return deviceContext;
}

void
DEVICECONTEXT_Destroy(
	PDEVICE_CONTEXT DeviceContext
	)
{
	FUNCTION_ENTRY;

	dev_dbg(dev_ctx_to_dev(DeviceContext), "\n");

	if (DeviceContext->WorkItemQueue) {
		destroy_workqueue(DeviceContext->WorkItemQueue);
		DeviceContext->WorkItemQueue = NULL;
	}

	kfree( DeviceContext );

	FUNCTION_LEAVE;
}

int
DEVICECONTEXT_ErrorCheck(
	PDEVICE_CONTEXT DeviceContext
	)
{
	int status = 0;

//        KOUT( "ErrChk: %ps:%ps:%ps: DeviceContext NULL\n",
//               __builtin_return_address(3),
//               __builtin_return_address(2),
//               __builtin_return_address(0) );
	if (NULL == DeviceContext){
		status = -ENODEV;
//      KOUT( "DEVICECONTEXT_ErrorCheck DeviceContext NULL\n");
		goto Exit;
	}

	if (NULL == DeviceContext->InterfaceBackup){
		status = -ENODEV;
//      KOUT( "DEVICECONTEXT_ErrorCheck DeviceContext->InterfaceBackup NULL\n");
		goto Exit;
	}

	if ((DeviceContext->InterfaceBackup->condition != USB_INTERFACE_BOUND) &&
		 (DeviceContext->InterfaceBackup->condition != USB_INTERFACE_BINDING)) {
//      KOUT( "DEVICECONTEXT_ErrorCheck InterfaceBackup->condition NULL\n");
		status = -ENODEV;
		goto Exit;
	}

	if (xhci_is_dying_safe(DeviceContext)) {
		status = -ENODEV;
		dev_dbg(dev_ctx_to_dev(DeviceContext), "ErrChk: XHCI_STATE_DYING!\n");
		goto Exit;
	}
	if ( DeviceContext->UsbContext.IsUsbHcdInvalid )
	{
		status = -ENODEV;
		dev_dbg(dev_ctx_to_dev(DeviceContext), "ErrChk: Usb hcd is invalid!\n");
		goto Exit;
	}

	if (USB_STATE_NOTATTACHED == DeviceContext->UsbContext.UsbDevice->state)
	{
		status = -ENODEV;
		dev_dbg(dev_ctx_to_dev(DeviceContext), "ErrChk: FL6000 is not attached!\n");
		goto Exit;
	}

	if ( DeviceContext->ErrorFlags.UrbContextAllocationError )
	{
		status = -ENODEV;
		dev_dbg(dev_ctx_to_dev(DeviceContext), "ErrChk: URB error!\n");
		goto Exit;
	}

	if ( DeviceContext->ErrorFlags.EmbeddedRegisterError )
	{
		status = -ENODEV;
		dev_dbg(dev_ctx_to_dev(DeviceContext), "ErrChk: Embedded register read/write error! \n");
		goto Exit;
	}

	if ( IS_EMBEDDED_CACHE_WRITE_ERROR( DeviceContext ) )
	{
		status = -ENODEV;
		dev_dbg(dev_ctx_to_dev(DeviceContext), "ErrChk: Embedded cache write error! \n");
		goto Exit;
	}

Exit:
	/* TODO EHUB Should we be calling usb_hc_died here to speed unplug? */

	return status;
}

