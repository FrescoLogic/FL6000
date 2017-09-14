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

#ifndef EHUB_MESSAGE_H
#define EHUB_MESSAGE_H

#define MESSAGE_TYPE_UNKNOWN                    ( 0 )
#define MESSAGE_TYPE_EMBEDDED_MEMORY_READ_COMPLETION        ( 1 )
#define MESSAGE_TYPE_EMBEDDED_MEMORY_WRITE_COMPLETION       ( 2 )
#define MESSAGE_TYPE_EMBEDDED_REGISTER_READ_COMPLETION      ( 3 )
#define MESSAGE_TYPE_EMBEDDED_EVENT_TRB         ( 4 )

typedef union _EMBEDDED_GENERIC_HEADER_
{
	struct
	{
		u32 Read:1;
		u32 Write:1;
		u32 Rsvd0:3;
		u32 RegAccess:1;
		u32 Rsvd1:18;
		u32 RequestId:3;
		u32 Rsvd2:5;
	};
	u32 Value;
} EMBEDDED_GENERIC_HEADER, *PEMBEDDED_GENERIC_HEADER;

void
MESSAGE_Parsing(
	PDEVICE_CONTEXT DeviceContext,
	u8* DataBuffer,
	int DataBufferLength
	);

int
MESSAGE_StartLoopBulk(
	PDEVICE_CONTEXT DeviceContext
	);

int
MESSAGE_StartLoopInterrupt(
	PDEVICE_CONTEXT DeviceContext
	);


#ifdef EHUB_ISOCH_ENABLE
int
MESSAGE_InitLoopIsoch(
	PDEVICE_CONTEXT DeviceContext
);

int
MESSAGE_StartLoopIsoch(
	PDEVICE_CONTEXT DeviceContext
);

void
MESSAGE_StopLoopIsoch(
	PDEVICE_CONTEXT DeviceContext
);

void
MESSAGE_delayed_StopLoopIsoch(
	struct work_struct* work
);

void
MESSAGE_queue_StopLoopIsoch(
	PDEVICE_CONTEXT DeviceContext
);

void
MESSAGE_FreeLoopIsoch(
	PDEVICE_CONTEXT DeviceContext
);
#endif /* EHUB_ISOCH_ENABLE */

void
MESSAGE_StopLoopBulk(
	PDEVICE_CONTEXT DeviceContext
	);

void
MESSAGE_StopLoopInterrupt(
	PDEVICE_CONTEXT DeviceContext
	);

#endif
