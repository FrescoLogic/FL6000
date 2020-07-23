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

#ifndef EHUB_REGISTER_H
#define EHUB_REGISTER_H

#define REGISTER_READ  ( 1 )
#define REGISTER_WRITE ( 0 )

#define NUMBER_OF_BYTES_IN_SETUP_PACKET    ( 8 )
#define NUMBER_OF_BYTES_IN_REGISTER_DATA   ( 4 )

#define CONTROL_ENDPOINT_I2C_CMD_READ   ( 64 )
#define CONTROL_ENDPOINT_I2C_CMD_WRITE  ( 65 )

int
REGISTER_Transfer(
	PDEVICE_CONTEXT DeviceContext,
	int ReadWrite,
	u32 Offset,
	u32* Data
	);

int
REGISTER_Write(
	PDEVICE_CONTEXT DeviceContext,
	u32 Offset,
	u32* Data
	);

int
REGISTER_Read(
	PDEVICE_CONTEXT DeviceContext,
	u32 Offset,
	u32* Data
	);

void
REGISTER_SetBit(
	PDEVICE_CONTEXT DeviceContext,
	u32 Offset,
	u32 BitOffset
	);

void
REGISTER_ClearBit(
	PDEVICE_CONTEXT DeviceContext,
	u32 Offset,
	u32 BitOffset
	);

#endif
