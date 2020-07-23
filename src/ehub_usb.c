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

#include "xhci.h"

#include "ehub_defines.h"

#include "ehub_device_context.h"
#include "ehub_message.h"
#include "ehub_utility.h"
#include "ehub_usb.h"

/* TODO remove all the pointless functions */

int
USB_InterfaceCreateBulk(
	PDEVICE_CONTEXT DeviceContext
)
{
	int status;

	FUNCTION_ENTRY;

	status = usb_set_interface(DeviceContext->UsbContext.UsbDevice,
							   EHUB_INTERFACE_NUMBER_BULK,
							   EHUB_INTERFACE_ALTERNATE_SETTING_BULK);
	if (status < 0)
	{
		dev_err(dev_ctx_to_dev(DeviceContext), "ERROR usb_set_interface fail! %d\n", status );
		goto Exit;
	}

	status = MESSAGE_StartLoopBulk( DeviceContext );

Exit:

	FUNCTION_LEAVE;

	return status;
}

int
USB_InterfaceCreateInterrupt(
	PDEVICE_CONTEXT DeviceContext
	)
{
	int status;

	FUNCTION_ENTRY;

	status = usb_set_interface( DeviceContext->UsbContext.UsbDevice,
								EHUB_INTERFACE_NUMBER_INTERRUPT,
								EHUB_INTERFACE_ALTERNATE_SETTING_INTERRUPT );
	if (status < 0) {
		dev_err(dev_ctx_to_dev(DeviceContext), "ERROR usb_set_interface fail! %d\n", status );
		goto Exit;
	}

	status = MESSAGE_StartLoopInterrupt( DeviceContext );

Exit:

	FUNCTION_LEAVE;

	return status;
}

int
USB_InterfaceDestroyBulk(
	PDEVICE_CONTEXT DeviceContext
	)
{
	MESSAGE_StopLoopBulk( DeviceContext );
	return 0;
}

int
USB_InterfaceDestroyInterrupt(
	PDEVICE_CONTEXT DeviceContext
	)
{
	MESSAGE_StopLoopInterrupt( DeviceContext );
	return 0;
}

int
USB_InterfaceSuspendBulk(
	PDEVICE_CONTEXT DeviceContext
	)
{
	MESSAGE_StopLoopBulk( DeviceContext );
	return 0;
}

int
USB_InterfaceSuspendInterrupt(
	PDEVICE_CONTEXT DeviceContext
	)
{
	MESSAGE_StopLoopInterrupt( DeviceContext );
	return 0;
}

int
USB_InterfaceResumeBulk(
	PDEVICE_CONTEXT DeviceContext
	)
{
	return MESSAGE_StartLoopBulk( DeviceContext );
}

int
USB_InterfaceResumeInterrupt(
	PDEVICE_CONTEXT DeviceContext
	)
{
	return MESSAGE_StartLoopInterrupt( DeviceContext );
}

#ifdef EHUB_ISOCH_ENABLE
int
USB_InterfaceCreateIsoch(
	PDEVICE_CONTEXT DeviceContext
)
{
	int status;

	FUNCTION_ENTRY;

	dev_warn(dev_ctx_to_dev(DeviceContext), "USB_InterfaceCreateIsoch: calling usb_set_interface\n" );

	status = usb_set_interface(DeviceContext->UsbContext.UsbDevice,
							   EHUB_INTERFACE_NUMBER_ISOCH,
							   EHUB_INTERFACE_ALTERNATE_SETTING_ISOCH);
	if (status < 0) {
		dev_err(dev_ctx_to_dev(DeviceContext), "ERROR usb_set_interface fail! %d\n", status);
		goto Exit;
	}

	status = MESSAGE_InitLoopIsoch( DeviceContext );

	if (status < 0) {
		dev_err(dev_ctx_to_dev(DeviceContext), "ERROR MESSAGE_InitLoopIsoch fail! %d\n", status);
		MESSAGE_FreeLoopIsoch(DeviceContext);
	}

Exit:

	FUNCTION_LEAVE;

	return status;
}

int
USB_InterfaceDestroyIsoch(
	PDEVICE_CONTEXT DeviceContext
)
{
	dev_warn(dev_ctx_to_dev(DeviceContext), "USB_InterfaceDestroyIsoch: calling MESSAGE_StopLoopIsoch\n" );
	MESSAGE_StopLoopIsoch(DeviceContext);
	MESSAGE_FreeLoopIsoch( DeviceContext );
	return 0;
}

int
USB_InterfaceSuspendIsoch(
	PDEVICE_CONTEXT DeviceContext
)
{
	if (dev_ctx_to_xhci(DeviceContext)->isoch_in_running)
		MESSAGE_StopLoopIsoch(DeviceContext);
	return 0;
}

int
USB_InterfaceResumeIsoch(
	PDEVICE_CONTEXT DeviceContext
)
{
	int status = 0;

	if (atomic_read( &dev_ctx_to_xhci(DeviceContext)->num_active_isoc_eps))
		status = MESSAGE_StartLoopIsoch( DeviceContext );

	return status;
}
#endif /* EHUB_ISOCH_ENABLE */
