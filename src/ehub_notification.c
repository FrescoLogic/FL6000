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
NOTIFICATION_Wait(
	PDEVICE_CONTEXT DeviceContext,
	struct completion* NotificationEvent,
	int ExpireInMs
	)
{
	int status = 0;
	u32 expire;
	long waited;

	expire = ExpireInMs ? msecs_to_jiffies( ExpireInMs ) : MAX_SCHEDULE_TIMEOUT;

#ifdef USE_FUNCTION_ENTRY
	dev_dbg(dev_ctx_to_dev(DeviceContext), "from %ps %dms\n",
			__builtin_return_address(0), jiffies_to_msecs(expire));
#endif

	status = DEVICECONTEXT_ErrorCheck(DeviceContext);
	if (status < 0) {
		return -ESHUTDOWN;
	}

	waited = wait_for_completion_interruptible_timeout( NotificationEvent, expire);
	if ( waited > 0)
	{
#ifdef USE_FUNCTION_LEAVE
		dev_dbg(dev_ctx_to_dev(DeviceContext), "from %ps waited %dms\n",
				__builtin_return_address(0), jiffies_to_msecs(waited));
#endif
	}
	else if (waited == 0)
	{
		status = -ETIMEDOUT;
		dev_err(dev_ctx_to_dev(DeviceContext), "from %ps TIMEOUT after %dms\n",
				__builtin_return_address(0), jiffies_to_msecs(expire));
	}
	else
	{
		status = -EINVAL;
		dev_err(dev_ctx_to_dev(DeviceContext), "ERROR from %ps %ld\n",
				__builtin_return_address(0), waited);
	}

	return status;
}

void
NOTIFICATION_Reset(
	struct completion* NotificationEvent
	)
{
	init_completion( NotificationEvent );
}

void
NOTIFICATION_Notify(
	PDEVICE_CONTEXT DeviceContext,
	struct completion* NotificationEvent
	)
{
	complete( NotificationEvent );
}
