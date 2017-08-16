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

#ifndef EHUB_EMBEDDED_REGISTER_H
#define EHUB_EMBEDDED_REGISTER_H

typedef union _EMBEDDED_REGISTER_COMMAND_
{
	struct
	{
		u32 Read:1;
		u32 Write:1;
		u32 Rsvd0:3;
		u32 RegAccess:1;
		u32 TrafficClass:2; // 0 = async, 1 = iso, 2 = intr
		u32 ByteEnables:8;  // Byte enables for 8 bytes of data LSbit == LSByte
		u32 Address:16;
	};
	u32 Value;
} EMBEDDED_REGISTER_COMMAND, *PEMBEDDED_REGISTER_COMMAND;

typedef union _EMBEDDED_REGISTER_DATA_RESPONSE_
{
	struct
	{
		EMBEDDED_REGISTER_COMMAND EmbeddedRegisterCommand;
		u32 Resv[ 2 ];
		u32 Data;
	};
	u32 Dwords[ 4 ];
} EMBEDDED_REGISTER_DATA_RESPONSE, *PEMBEDDED_REGISTER_DATA_RESPONSE;

typedef union _EMBEDDED_REGISTER_DATA_TRANSFER_
{
	struct
	{
		EMBEDDED_REGISTER_COMMAND EmbeddedRegisterCommand;
		u32 Data;
	};
	u32 Dwords[ 2 ];
} EMBEDDED_REGISTER_DATA_TRANSFER, *PEMBEDDED_REGISTER_DATA_TRANSFER;

int
EMBEDDED_REGISTER_Read(
	PDEVICE_CONTEXT DeviceContext,
	u32 Address,
	u32* Data
	);

int
EMBEDDED_REGISTER_Write(
	PDEVICE_CONTEXT DeviceContext,
	u32 Address,
	u32* Data
	);

int
EMBEDDED_REGISTER_Write_Doorbell(
	PDEVICE_CONTEXT DeviceContext,
	u32 Address,
	u32* Data
	);

#endif
