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

#ifndef EHUB_EMBEDDED_CACHE_H
#define EHUB_EMBEDDED_CACHE_H

#define EHUB_CACHE_SIZE             ( 0x20000 )
#define EHUB_CACHE_BLOCK_SIZE       ( 0x400 )
#define EHUB_CACHE_NUMBER_OF_BLOCKS ( EHUB_CACHE_SIZE / EHUB_CACHE_BLOCK_SIZE )
/*
 * Start with a reasonable number.
 * More entries will be allocated on the fly if needed.
 */
#define EHUB_CACHE_QUEUE_ENTRIES    ( 32 )

/*
 * Maximum number of cache blocks to use for caching data
 */
#define EHUB_CACHE_MAX_DATA_BLOCKS  ( 6 )

#define EHUB_CACHE_START_ADDRESS            ( 0x1000 )
//#define EHUB_CACHE_INITIALIZATION_REGISTER  ( 0x22020FFF )
#define EHUB_EVENTS_ON_ISOCH  ( 1 << 27 )
#define EHUB_CACHE_INITIALIZATION_REGISTER  ( 0x22020FFF | EHUB_EVENTS_ON_ISOCH )

#define EMBEDDED_HOST_MFINDEX_REG_ADDRESS           ( 0x2000 )

#define DEVHOST_CACHE_BAR_REG_ADDRESS               ( 0x81EC )
#define DEVHOST_XHCI_PAGESIZE_REG_ADDRESS           ( 0x0088 )

#define LAST_TRB_INDEX ( TRBS_PER_SEGMENT - 2 )
#define LINK_TRB_INDEX ( TRBS_PER_SEGMENT - 1 )

#define trb_ptr_to_addr(l,h)    ((__u64)( h << 32 | l))

typedef union _EMBEDDED_MEMORY_COMMAND_
{
	struct
	{
		u32 Read:1;
		u32 Write:1;
		u32 Rsvd0:1;
		u32 MSI:1;          // 1 = this is an interrupt MSI write, 0 = everything else
		u32 Cache:1;
		u32 RegAccess:1;
		u32 TrafficClass:2; // 0 = async, 1 = iso, 2 = intr
		u32 Length:16;      // Byte enables for 8 bytes of data LSbit == LSByte
		u32 RequestId:3;    // Request ID
		u32 Rsvd2:5;        // Request ID
	};
	u32 Value;
} EMBEDDED_MEMORY_COMMAND, *PEMBEDDED_MEMORY_COMMAND;

typedef union _EMBEDDED_MEMORY_TRANSFER_
{
	struct
	{
		EMBEDDED_MEMORY_COMMAND EmbeddedMemoryCommand;
		u32 AddressLow;
		u32 AddressHigh;
	};
	u32 Dwords[ 3 ];
} EMBEDDED_MEMORY_TRANSFER, *PEMBEDDED_MEMORY_TRANSFER;

typedef union _EMBEDDED_CACHE_TRANSFER_
{
	struct
	{
		EMBEDDED_MEMORY_COMMAND EmbeddedMemoryCommand;
		u32 Address;
	};
	u32 Dwords[ 2 ];
} EMBEDDED_CACHE_TRANSFER, *PEMBEDDED_CACHE_TRANSFER;

typedef struct _EHUB_CACHE_BLOCK_
{
	struct list_head list;
	struct urb* urb;
	u32 IndexOfBlock;
	u32 Address;
} EHUB_CACHE_BLOCK, *PEHUB_CACHE_BLOCK;

typedef struct _EHUB_CACHE_TRB_
{
	struct list_head list;
	u64 AddrCache;
	u32 TrbDataField[ 4 ];
} EHUB_CACHE_TRB, *PEHUB_CACHE_TRB;

int
EMBEDDED_CACHE_Write(
	PDEVICE_CONTEXT DeviceContext,
	URB_CONTEXT *UrbContext,
	u32 CacheAddress,
	u32* DataBuffer,
	u32 DataBufferLength,
	int TrbCycleState
	);

#endif
