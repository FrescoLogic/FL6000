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

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/usb.h>
#include <linux/uaccess.h>

#include "ehub_defines.h"

#include "ehub_utility.h"
#include "ehub_device_context.h"
#include "ehub_embedded_register.h"
#include "ehub_embedded_cache.h"
#include "ehub_message.h"
#include "ehub_notification.h"
#include "ehub_urb.h"
#include "ehub_work_item.h"
#include "xhci.h"

void
MESSAGE_HandleMessage_EMBEDDED_MEMORY_READ_COMPLETION(
	PDEVICE_CONTEXT DeviceContext,
	PEMBEDDED_GENERIC_HEADER EmbeddedGenericHeader
)
{
	int status;
	PEMBEDDED_MEMORY_TRANSFER embeddedMemoryTransfer;
	int dataBufferLength;
	u8* addressTarget;
	PURB_CONTEXT urbContext;

	//   FUNCTION_ENTRY;

	embeddedMemoryTransfer = ( PEMBEDDED_MEMORY_TRANSFER )EmbeddedGenericHeader;
	addressTarget = ( u8* )trb_ptr_to_addr(embeddedMemoryTransfer->AddressLow, ( u64 )embeddedMemoryTransfer->AddressHigh);

	status = DEVICECONTEXT_ErrorCheck(DeviceContext);
	if (status < 0) {
		dev_dbg(dev_ctx_to_dev(DeviceContext), "ErrorCheck FAILED %d\n", status);
		goto Exit;
	}

	might_sleep();
	if (mutex_lock_interruptible(&DeviceContext->MessageHandleEmbeddedMemoryReadCompletionLock))             \
		KERNEL_PANIC("mutex_lock_interruptible fail!");

	urbContext = DeviceContext->UrbContextEmbeddedMemoryReadCompletion[DeviceContext->CurrentEmbeddedMemoryReadCompletionUrbContextIndex];
	DeviceContext->CurrentEmbeddedMemoryReadCompletionUrbContextIndex++;
	DeviceContext->CurrentEmbeddedMemoryReadCompletionUrbContextIndex %= NUMBER_OF_MESSAGE_BULK;

	mutex_unlock(&DeviceContext->MessageHandleEmbeddedMemoryReadCompletionLock);

	if (!urbContext) {
		dev_dbg(dev_ctx_to_dev(DeviceContext), "EMBEDDED_MEMORY_READ_COMPLETION: urbContext NULL\n");
		goto Exit;
	}

	dataBufferLength = sizeof( EMBEDDED_MEMORY_COMMAND ) + embeddedMemoryTransfer->EmbeddedMemoryCommand.Length;

	urbContext->Urb->transfer_buffer_length = dataBufferLength;

	memcpy(urbContext->DataBuffer, ( u8* )embeddedMemoryTransfer, sizeof(EMBEDDED_MEMORY_COMMAND));

	status = probe_kernel_read(urbContext->DataBuffer + sizeof(EMBEDDED_MEMORY_COMMAND),
							   addressTarget,
							   embeddedMemoryTransfer->EmbeddedMemoryCommand.Length);
	if (status < 0) {
		dev_info(dev_ctx_to_dev(DeviceContext), "probe_kernel_read returned %d\n", status);
		goto Exit;
	}

	status = URB_Submit( urbContext );
	if (status < 0)
	{
		dev_err(dev_ctx_to_dev(DeviceContext), "ERROR: URB_Submit fail! %d\n", status );
		goto Exit;
	}
	dev_dbg(dev_ctx_to_dev(DeviceContext), "0x%0X bytes @0x%p\n", embeddedMemoryTransfer->EmbeddedMemoryCommand.Length, addressTarget );

Exit:
	;
//    FUNCTION_LEAVE;
}

void
MESSAGE_HandleMessage_EMBEDDED_EVENT_TRB(
	PDEVICE_CONTEXT DeviceContext,
	PEMBEDDED_GENERIC_HEADER EmbeddedGenericHeader
	)
{
	struct xhci_generic_trb *eventTrb;
	u8* addressTarget;

//    FUNCTION_ENTRY;

	addressTarget = (u8 *)EmbeddedGenericHeader + sizeof( EMBEDDED_MEMORY_TRANSFER );
	eventTrb = (struct xhci_generic_trb *)( addressTarget );

	dev_dbg(dev_ctx_to_dev(DeviceContext), ">> [0]0x%08x [1]0x%08x [2]0x%08x [3]0x%08x\n",
			eventTrb->field[0],
			eventTrb->field[1],
			eventTrb->field[2],
			eventTrb->field[3]);

	if ( DeviceContext->ControlFlags.IsEhubXhciInitReady )
	{
		ehub_xhci_handle_event( DeviceContext->UsbContext.xhci_hcd, *eventTrb );
	}
	else
	{
		dev_dbg(dev_ctx_to_dev(DeviceContext), "Bypass event!\n" );
	}

	FUNCTION_LEAVE;
}

void
MESSAGE_HandleMessage_EMBEDDED_REGISTER_READ_COMPLETION(
	PDEVICE_CONTEXT DeviceContext,
	PEMBEDDED_GENERIC_HEADER EmbeddedGenericHeader
	)
{
	PEMBEDDED_REGISTER_DATA_RESPONSE embeddedRegisterTransfer;

//    FUNCTION_ENTRY;

	embeddedRegisterTransfer = ( PEMBEDDED_REGISTER_DATA_RESPONSE )EmbeddedGenericHeader;

	DeviceContext->DataEmbeddedRegisterRead = embeddedRegisterTransfer->Data;

	dev_dbg(dev_ctx_to_dev(DeviceContext), "Read 0x%0X from 0x%X\n",
			DeviceContext->DataEmbeddedRegisterRead,
			embeddedRegisterTransfer->Dwords[1]);

	if (EMBEDDED_HOST_MFINDEX_REG_ADDRESS == embeddedRegisterTransfer->Dwords[1]) {
		u64 usecs64;
		int usecs;
		unsigned int calc_uf_cnt;
		int calc_uf_err;

		DeviceContext->pending_uframe_time = ktime_get();
		usecs64 = ktime_to_ns(ktime_sub(DeviceContext->pending_uframe_time, DeviceContext->last_uframe_time));
		do_div(usecs64, NSEC_PER_USEC);
		usecs = usecs64;
		calc_uf_cnt = DeviceContext->last_uframe_cnt + (usecs / 125);
		calc_uf_err = calc_uf_cnt - embeddedRegisterTransfer->Data;

		xhci_dbg(dev_ctx_to_xhci(DeviceContext),
				 "cur_uf=0x%0X calc_uf=0x%0X uf_delta=%d usec_delta=%0ld\n",
				 embeddedRegisterTransfer->Data, calc_uf_cnt, calc_uf_err,
				 ( long int )ktime_us_delta(DeviceContext->pending_uframe_time, DeviceContext->last_uframe_time));

		DeviceContext->last_uframe_time = DeviceContext->pending_uframe_time;
		DeviceContext->last_uframe_cnt = embeddedRegisterTransfer->Data;
	}


	NOTIFICATION_Notify( DeviceContext,
						 &DeviceContext->CompletionEventEmbeddedRegisterRead );

//    FUNCTION_LEAVE;
}

void
MESSAGE_HandleMessage_EMBEDDED_MEMORY_WRITE_COMPLETION(
	PDEVICE_CONTEXT DeviceContext,
	PEMBEDDED_GENERIC_HEADER EmbeddedGenericHeader
	)
{
	PEMBEDDED_MEMORY_TRANSFER embeddedMemoryTransfer;
	u8* addressTarget;
	u8* addressSource;
	int length;
	int status;

	//FUNCTION_ENTRY;

	embeddedMemoryTransfer = ( PEMBEDDED_MEMORY_TRANSFER )EmbeddedGenericHeader;
	addressTarget = (u8*)trb_ptr_to_addr(embeddedMemoryTransfer->AddressLow,(u64)embeddedMemoryTransfer->AddressHigh );

	length = embeddedMemoryTransfer->EmbeddedMemoryCommand.Length;

	addressSource = ( u8* )embeddedMemoryTransfer + sizeof( EMBEDDED_MEMORY_TRANSFER );

	if (((u64)addressTarget < EHUB_MIN_VALID_VADDR ) || (addressSource == NULL)) {
		dev_warn(dev_ctx_to_dev(DeviceContext),
				 "WARNING: Mem Write Completion source=0x%p target=0x%p length=0x%X\n",
				 addressSource, addressTarget, length);
		return;
	}

	status = probe_kernel_write(addressTarget,
								addressSource,
								length);
	if (status < 0) {
		dev_err(dev_ctx_to_dev(DeviceContext), "ERROR: probe_kernel_write returned %d\n", status);
	}

	dev_dbg(dev_ctx_to_dev(DeviceContext), "0x%0X bytes @0x%p\n", length, addressSource );

	//FUNCTION_LEAVE;
}

void
MESSAGE_Handle(
	PDEVICE_CONTEXT DeviceContext,
	PEMBEDDED_GENERIC_HEADER EmbeddedGenericHeader,
	int MessageType
	)
{
	switch ( MessageType )
	{
		case MESSAGE_TYPE_EMBEDDED_REGISTER_READ_COMPLETION:
		{
			MESSAGE_HandleMessage_EMBEDDED_REGISTER_READ_COMPLETION(DeviceContext,
																	EmbeddedGenericHeader);
			break;
		}
		case MESSAGE_TYPE_EMBEDDED_MEMORY_READ_COMPLETION:
		{
			MESSAGE_HandleMessage_EMBEDDED_MEMORY_READ_COMPLETION(DeviceContext,
																  EmbeddedGenericHeader);
			break;
		}
		case MESSAGE_TYPE_EMBEDDED_EVENT_TRB:
		{
			MESSAGE_HandleMessage_EMBEDDED_EVENT_TRB(DeviceContext,
													 EmbeddedGenericHeader);
			break;
		}
		case MESSAGE_TYPE_EMBEDDED_MEMORY_WRITE_COMPLETION:
		{
			MESSAGE_HandleMessage_EMBEDDED_MEMORY_WRITE_COMPLETION(DeviceContext,
																   EmbeddedGenericHeader);
			break;
		}
		case MESSAGE_TYPE_UNKNOWN:
		default:
		{
			dev_err(dev_ctx_to_dev(DeviceContext), "ERROR MESSAGE_TYPE_UNKNOWN\n" );
			ASSERT( false );
			break;
		}
	}
}

void
MESSAGE_GetType(
	PEMBEDDED_GENERIC_HEADER EmbeddedGenericHeader,
	int* MessageType,
	int* MessageLength
	)
{
	int messageType;
	int messageLength;

	if ( EmbeddedGenericHeader->RegAccess )
	{
		messageType = MESSAGE_TYPE_EMBEDDED_REGISTER_READ_COMPLETION;
		messageLength = sizeof( EMBEDDED_REGISTER_DATA_RESPONSE );
	}
	else if ( EmbeddedGenericHeader->Read )
	{
		messageType = MESSAGE_TYPE_EMBEDDED_MEMORY_READ_COMPLETION;
		messageLength = sizeof( EMBEDDED_MEMORY_TRANSFER );
	}
	else if ( EmbeddedGenericHeader->Write )
	{
		PEMBEDDED_MEMORY_COMMAND embeddedMemoryCommand;

		embeddedMemoryCommand = ( PEMBEDDED_MEMORY_COMMAND )EmbeddedGenericHeader;

		if ( embeddedMemoryCommand->RequestId == MEM_WR_TAG_EVT_MGR )
		{
			messageType = MESSAGE_TYPE_EMBEDDED_EVENT_TRB;
		}
		else
		{
			messageType = MESSAGE_TYPE_EMBEDDED_MEMORY_WRITE_COMPLETION;
		}

		messageLength = sizeof( EMBEDDED_MEMORY_TRANSFER );
		messageLength += embeddedMemoryCommand->Length;
	}
	else
	{
		messageType = MESSAGE_TYPE_UNKNOWN;
		messageLength = 0;
	}

	*MessageType = messageType;
	*MessageLength = messageLength;
}

void
MESSAGE_Parsing(
	PDEVICE_CONTEXT DeviceContext,
	u8* DataBuffer,
	int DataBufferLength
	)
{
	int status;
	PEMBEDDED_GENERIC_HEADER embeddedGenericHeader;
	int messageType;
	int messageLength;
	int parsingLength;

	messageType = MESSAGE_TYPE_UNKNOWN;
	messageLength = 0;
	parsingLength = 0;

	do
	{
		embeddedGenericHeader = ( PEMBEDDED_GENERIC_HEADER )( DataBuffer + parsingLength );

		if ( embeddedGenericHeader == NULL ||
			 embeddedGenericHeader->Value == 0 )
		{
			break;
		}

		MESSAGE_GetType( embeddedGenericHeader,
						 &messageType,
						 &messageLength );
		if (  MESSAGE_TYPE_UNKNOWN == messageType )
		{
			dev_info(dev_ctx_to_dev(DeviceContext), "MessageTypeUnknown\n" );
			break;
		}

		status = DEVICECONTEXT_ErrorCheck( DeviceContext );
		if (status < 0)
		{
			break;
		}

		MESSAGE_Handle( DeviceContext,
						embeddedGenericHeader,
						messageType );
		parsingLength += roundup( messageLength, 4 );
	} while (parsingLength < DataBufferLength );
}

int
MESSAGE_StartLoopBulk(
	PDEVICE_CONTEXT DeviceContext
	)
{
	int indexOfMessage;
	int status = 0;
	PURB_CONTEXT urbContext;

	FUNCTION_ENTRY;

	for ( indexOfMessage = 0; indexOfMessage < NUMBER_OF_MESSAGE_BULK; indexOfMessage++ )
	{
		urbContext = URB_Create(DeviceContext,
								DeviceContext->UsbContext.UsbDevice,
								DeviceContext->UsbContext.UsbPipeBulkIn,
								MESSAGE_DATA_BUFFER_SIZE_BULK,
								URB_CompletionRoutine_GetMessage,
								NULL,
								GFP_KERNEL);
		if ( NULL == urbContext )
		{
			status = -ENOMEM;
			dev_err(dev_ctx_to_dev(DeviceContext), "ERROR URB_Create fail for Bulk! %d\n", status );
			goto Exit;
		}
		else
		{
			DeviceContext->UrbContextMessageBulk[ indexOfMessage ] = urbContext;
		}

		status = URB_Submit( urbContext );
		if (status < 0)
		{
			dev_err(dev_ctx_to_dev(DeviceContext), "ERROR URB_Submit fail! %d\n", status );
			URB_Destroy( urbContext );
			goto Exit;
		}
	}

Exit:

	FUNCTION_LEAVE;

	return status;
}

int
MESSAGE_StartLoopInterrupt(
	PDEVICE_CONTEXT DeviceContext
	)
{
	int indexOfMessage;
	int status = 0;
	PURB_CONTEXT urbContext;

	FUNCTION_ENTRY;

	for ( indexOfMessage = 0; indexOfMessage < NUMBER_OF_MESSAGE_INTERRUPT; indexOfMessage++ )
	{
		urbContext = URB_Create(DeviceContext,
								DeviceContext->UsbContext.UsbDevice,
								DeviceContext->UsbContext.UsbPipeInterruptIn,
								MESSAGE_DATA_BUFFER_SIZE_INTERRUPT,
								URB_CompletionRoutine_GetMessage,
								NULL,
								GFP_KERNEL);
		if ( NULL == urbContext )
		{
			status = -ENOMEM;
			dev_err(dev_ctx_to_dev(DeviceContext), "ERROR URB_Create fail for Interrupt! %d\n", status );
			goto Exit;
		}
		else
		{
			DeviceContext->UrbContextMessageInterrupt[ indexOfMessage ] = urbContext;
		}

		dev_dbg(dev_ctx_to_dev(DeviceContext), "urbContext=0x%p\n", urbContext );

		status = URB_Submit( urbContext );
		if ( status < 0 ) {
			dev_err(dev_ctx_to_dev(DeviceContext), "ERROR URB_Submit fail! %d\n", status );
			URB_Destroy( urbContext );
			goto Exit;
		}
	}

Exit:

	FUNCTION_LEAVE;

	return status;
}

#ifdef EHUB_ISOCH_ENABLE
int
MESSAGE_InitLoopIsoch(
	PDEVICE_CONTEXT DeviceContext
)
{
	int indexOfMessage;
	int status = 0;
	PURB_CONTEXT urbContext;

	FUNCTION_ENTRY;

	dev_info(dev_ctx_to_dev(DeviceContext), "MESSAGE_InitLoopIsoch\n");

	for (indexOfMessage = 0; indexOfMessage < NUMBER_OF_MESSAGE_ISOCH; indexOfMessage++) {
		dev_dbg(dev_ctx_to_dev(DeviceContext), "CreateIsoch idx=%d\n",indexOfMessage);
		urbContext = URB_CreateIsoch(DeviceContext,
									 DeviceContext->UsbContext.UsbDevice,
									 DeviceContext->UsbContext.UsbPipeIsochIn,
									 MESSAGE_DATA_BUFFER_SIZE_ISOCH,
									 URB_CompletionRoutine_GetMessage,
									 GFP_KERNEL);
		if (NULL == urbContext) {
			dev_err(dev_ctx_to_dev(DeviceContext), "ERROR: CreateIsoch idx=%d failed\n", indexOfMessage);
			status = -ENOMEM;
			goto Exit;
		} else {
			DeviceContext->UrbContextMessageIsoch[indexOfMessage] = urbContext;
		}

		dev_dbg(dev_ctx_to_dev(DeviceContext), "urbContext=0x%p\n", urbContext);

	}

Exit:

	FUNCTION_LEAVE;

	return status;
}

int
MESSAGE_StartLoopIsoch(
	PDEVICE_CONTEXT DeviceContext
)
{
	int indexOfMessage;
	int status = 0;
	PURB_CONTEXT urbContext;

	FUNCTION_ENTRY;

	dev_info(dev_ctx_to_dev(DeviceContext), "MESSAGE_StartLoopIsoch from %ps\n", __builtin_return_address(0));

	if (delayed_work_pending(&DeviceContext->stop_isoch_work)) {
		bool cancelled = false;
		cancelled = cancel_delayed_work_sync(&DeviceContext->stop_isoch_work);
		dev_info(dev_ctx_to_dev(DeviceContext), "cancel_delayed_work_sync returned %d\n", cancelled);

	}

	if (dev_ctx_to_xhci(DeviceContext)->isoch_in_running) {
		dev_info(dev_ctx_to_dev(DeviceContext), "Isoch already running\n");
		goto Exit;
	}

	for (indexOfMessage = 0; indexOfMessage < NUMBER_OF_MESSAGE_ISOCH; indexOfMessage++) {
		urbContext = DeviceContext->UrbContextMessageIsoch[indexOfMessage];

		dev_dbg(dev_ctx_to_dev(DeviceContext), "StartIsoch idx=%d urbContext=0x%p\n",indexOfMessage, urbContext);

		status = URB_Submit(urbContext);
		if (status < 0) {
			dev_err(dev_ctx_to_dev(DeviceContext), "ERROR URB_Submit fail! %d\n", status);
			URB_Destroy(urbContext);
			goto Exit;
		}
	}

	dev_info(dev_ctx_to_dev(DeviceContext), "MESSAGE_StartLoopIsoch set isoch_in_running true\n");
	dev_ctx_to_xhci(DeviceContext)->isoch_in_running = true;

Exit:

	FUNCTION_LEAVE;

	return status;
}
#endif /* EHUB_ISOCH_ENABLE */

void
MESSAGE_StopLoopBulk(
	PDEVICE_CONTEXT DeviceContext
	)
{
	int indexOfMessage;
	PURB_CONTEXT urbContext;

	FUNCTION_ENTRY;

	for ( indexOfMessage = 0; indexOfMessage < NUMBER_OF_MESSAGE_BULK; indexOfMessage++ )
	{
		urbContext = DeviceContext->UrbContextMessageBulk[ indexOfMessage ];
		DeviceContext->UrbContextMessageBulk[indexOfMessage] = NULL;

		dev_dbg(dev_ctx_to_dev(DeviceContext), "urbContext=0x%p\n", urbContext);

		if ( NULL != urbContext )
		{
			NOTIFICATION_Notify( DeviceContext,
								 &urbContext->Event );
			usb_kill_urb( urbContext->Urb );
			URB_Destroy( urbContext );
		}
	}

	FUNCTION_LEAVE;
}

void
MESSAGE_StopLoopInterrupt(
	PDEVICE_CONTEXT DeviceContext
	)
{
	int indexOfMessage;
	PURB_CONTEXT urbContext;

	for ( indexOfMessage = 0; indexOfMessage < NUMBER_OF_MESSAGE_INTERRUPT; indexOfMessage++ )
	{
		urbContext = DeviceContext->UrbContextMessageInterrupt[ indexOfMessage ];
		DeviceContext->UrbContextMessageInterrupt[indexOfMessage] = NULL;
		if ( NULL != urbContext )
		{
			NOTIFICATION_Notify( DeviceContext,
								 &urbContext->Event );
			urbContext->Status = URB_STATUS_COMPLETE;
			usb_kill_urb( urbContext->Urb );
			URB_Destroy( urbContext );
		}
	}
}

#ifdef EHUB_ISOCH_ENABLE
void
MESSAGE_StopLoopIsoch(
	PDEVICE_CONTEXT DeviceContext
)
{
	int indexOfMessage;
	PURB_CONTEXT urbContext;

	dev_info(dev_ctx_to_dev(DeviceContext), "MESSAGE_StopLoopIsoch\n" );
	if (!dev_ctx_to_xhci(DeviceContext)->isoch_in_running) {
		dev_info(dev_ctx_to_dev(DeviceContext), "Isoch not running\n");
		return;
	}

for (indexOfMessage = 0; indexOfMessage < NUMBER_OF_MESSAGE_ISOCH; indexOfMessage++) {
		urbContext = DeviceContext->UrbContextMessageIsoch[indexOfMessage];
		if (NULL != urbContext) {
			NOTIFICATION_Notify(DeviceContext,
								&urbContext->Event);
			urbContext->Status = URB_STATUS_COMPLETE;
			usb_kill_urb(urbContext->Urb);
		}
	}
	dev_info(dev_ctx_to_dev(DeviceContext), "MESSAGE_StopLoopIsoch clear isoch_in_running false\n");
	dev_ctx_to_xhci(DeviceContext)->isoch_in_running = false;
}

void
MESSAGE_delayed_StopLoopIsoch(
	struct work_struct* work
)
{
	PDEVICE_CONTEXT DeviceContext =
		container_of(work, DEVICE_CONTEXT, stop_isoch_work.work);

	dev_info(dev_ctx_to_dev(DeviceContext),
			 "MESSAGE_delayed_StopLoopIsoch DC=0x%p\n", DeviceContext );

	MESSAGE_StopLoopIsoch(DeviceContext);
}

void
MESSAGE_queue_StopLoopIsoch(
	PDEVICE_CONTEXT DeviceContext
)
{
	dev_info(dev_ctx_to_dev(DeviceContext),
			 "MESSAGE_queue_StopLoopIsoch DC=0x%p\n", DeviceContext);

	if (delayed_work_pending(&DeviceContext->stop_isoch_work)) {
		dev_info(dev_ctx_to_dev(DeviceContext), "INFO: StopLoopIsoch already pending!\n");
		return;
	}

	INIT_DELAYED_WORK(&DeviceContext->stop_isoch_work, MESSAGE_delayed_StopLoopIsoch);

	/* Wait 100ms before stopping in case we need to start again.
	 * This can happen during device initialization for usb audio devices */
	schedule_delayed_work(&DeviceContext->stop_isoch_work, 10);
}

void
MESSAGE_FreeLoopIsoch(
	PDEVICE_CONTEXT DeviceContext
)
{
	int indexOfMessage;
	PURB_CONTEXT urbContext;

	dev_info(dev_ctx_to_dev(DeviceContext), "MESSAGE_FreeLoopIsoch\n");
	for (indexOfMessage = 0; indexOfMessage < NUMBER_OF_MESSAGE_ISOCH; indexOfMessage++) {
		urbContext = DeviceContext->UrbContextMessageIsoch[indexOfMessage];
		if (NULL != urbContext) {
			URB_Destroy(urbContext);
		}
	}
}
#endif /* EHUB_ISOCH_ENABLE */
