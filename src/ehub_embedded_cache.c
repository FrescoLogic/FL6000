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

int
EMBEDDED_CACHE_Write(
	PDEVICE_CONTEXT DeviceContext,
	URB_CONTEXT *UrbContext,
	u32 CacheAddress,
	u32* DataBuffer,
	u32 DataBufferLength,
	int TrbCycleState
	)
{
	PEMBEDDED_CACHE_TRANSFER embeddedCacheTransfer;
	u32 alignment;
	u32 dataBufferLengthAligned;
	u32 transferLength;
	int status;
	u8* tempBuffer;

	FUNCTION_ENTRY;

	if (UrbContext->Status != URB_STATUS_FREE && UrbContext->Status != URB_STATUS_COMPLETE)
		dev_err(dev_ctx_to_dev(DeviceContext), "ERROR UrbContext->Status=%d not expected\n", UrbContext->Status);

	dev_dbg(dev_ctx_to_dev(DeviceContext),
			 "EMBEDDED_CACHE_Write from %ps CA=0x%08x DP=0x%pk, Len=0x%0X \n",
				__builtin_return_address ( 0 ),
			 CacheAddress, DataBuffer, DataBufferLength);

	status = DEVICECONTEXT_ErrorCheck( DeviceContext );
	if ( status < 0 )
	{
		goto Exit;
	}

	if ( CacheAddress & 0x7 )
	{
		status = -EINVAL;
		dev_err(dev_ctx_to_dev(DeviceContext), "ERROR CacheAddress=0x%X not qword aligned\n", CacheAddress );
		goto Exit;
	}

	if ( DataBufferLength & 0x7 )
	{
		alignment = 8 - ( DataBufferLength % 8 );
	}
	else
	{
		alignment = 0;
	}

	dataBufferLengthAligned = DataBufferLength + alignment;

	transferLength = sizeof( EMBEDDED_CACHE_TRANSFER );
	transferLength += dataBufferLengthAligned;

	ASSERT(transferLength <= UrbContext->DataBufferLength);
	UrbContext->Urb->transfer_buffer_length = transferLength;

	embeddedCacheTransfer = ( PEMBEDDED_CACHE_TRANSFER )UrbContext->DataBuffer;

	embeddedCacheTransfer->Address = CacheAddress;
	embeddedCacheTransfer->EmbeddedMemoryCommand.Length = dataBufferLengthAligned >> 3;
	embeddedCacheTransfer->EmbeddedMemoryCommand.Cache = true;

	tempBuffer = ( u8* )( ( u64 )UrbContext->DataBuffer + ( u64 )sizeof( EMBEDDED_CACHE_TRANSFER ) );

	if (NULL != DataBuffer) {
		memcpy(tempBuffer, DataBuffer, DataBufferLength);
	} else {
		dev_dbg(dev_ctx_to_dev(DeviceContext), "memset dest=0x%p len=%d\n", tempBuffer, DataBufferLength);
		memset(tempBuffer, 0, DataBufferLength);

		if (TrbCycleState == 0) {
			struct xhci_link_trb *trb;
			int index;

			trb = (struct xhci_link_trb*)tempBuffer;

			for (index = 0; index < TRBS_PER_SEGMENT; index++) {
				trb->control |= TRB_CYCLE;
				trb += sizeof(struct xhci_link_trb);
			}
		}
	}

	status = URB_Submit( UrbContext );
	if ( status < 0 ) {
		dev_err(dev_ctx_to_dev(DeviceContext), "ERROR URB_Submit fail! %d\n", status);
		goto Exit;
	}

Exit:

	if ( status < 0 ) {
		DeviceContext->ErrorFlags.EmbeddedCacheWriteError = 1;
	}

	FUNCTION_LEAVE;

	return status;
}
