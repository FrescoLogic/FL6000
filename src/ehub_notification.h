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

#ifndef EHUB_NOTIFICATION_H
#define EHUB_NOTIFICATION_H

#define NOTIFICATION_EVENT_TIMEOUT ( 500 )
#define POLLING_INTERVAL_IN_MS     ( 10 )

typedef enum _MEM_WR_TAG_ENUM_
{
	MEM_WR_TAG_EVT_MGR          = 0,
	MEM_WR_TAG_IDMA             = 1,
	MEM_WR_TAG_CNTX_MGR         = 2,
	MEM_WR_TAG_DBG_PORT_EVT     = 3,
	MEM_WR_TAG_DBG_PORT_IDMA    = 4,
	MEM_WR_TAG_TRM_CACHE        = 5,
	MEM_WR_TAG_MAX
} MEM_WR_TAG_ENUM, *PMEM_WR_TAG_ENUM;

int
NOTIFICATION_Wait(
	PDEVICE_CONTEXT DeviceContext,
	struct completion* NotificationEvent,
	int ExpireInMs
	);

void
NOTIFICATION_Reset(
	struct completion* NotificationEvent
	);

void
NOTIFICATION_Notify(
	PDEVICE_CONTEXT DeviceContext,
	struct completion* NotificationEvent
	);

#endif
