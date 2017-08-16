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

#ifndef EHUB_WORK_ITEM_H
#define EHUB_WORK_ITEM_H

#define MAX_NUMBER_OF_WORK_ITEM_PENDING_QUEUE_ITEM 64

PWORK_ITEM_CONTEXT
WORK_ITEM_Create(
	PDEVICE_CONTEXT DeviceContext,
	void ( *WorkItemProcess )( struct work_struct * ),
	int DataBufferLength
	);

void
WORK_ITEM_Destroy(
	PWORK_ITEM_CONTEXT WorkItemContext
	);

void
WORK_ITEM_Submit(
	PWORK_ITEM_CONTEXT WorkItemContext
);

void
WORK_ITEM_Process_MessageHandle(
	struct work_struct* WorkItem
	);

#endif
