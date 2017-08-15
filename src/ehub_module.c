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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/pm.h>

#include "ehub_defines.h"

#include "xhci.h"
#include "ehub_embedded_register.h"
#include "ehub_work_item.h"
#include "ehub_urb.h"
#include "ehub_usb.h"
#include "ehub_module.h"


MODULE_AUTHOR( STRING_MODULE_AUTHOR );
MODULE_DESCRIPTION( STRING_MODULE_DESCRIPTION );
MODULE_LICENSE( STRING_MODULE_LICENSE );

#define DRIVER_AUTHOR STRING_MODULE_AUTHOR
#define DRIVER_DESC STRING_MODULE_DESCRIPTION

/* used when updating list of hcds */
//DEFINE_MUTEX(ehub_bus_lock);  /* exported only for usbfs */

static struct usb_device_id global_ehub_usb_device_id[] =
{
    {
        .idVendor           = EHUB_DEVICE_VENDOR,
        .idProduct          = EHUB_DEVICE_PRODUCT,
        .bInterfaceClass    = EHUB_INTERFACE_CLASS,
        .bInterfaceSubClass = EHUB_INTERFACE_SUBCLASS,
        .match_flags = USB_DEVICE_ID_MATCH_VENDOR |
                       USB_DEVICE_ID_MATCH_PRODUCT |
                       USB_DEVICE_ID_MATCH_INT_CLASS |
                       USB_DEVICE_ID_MATCH_INT_SUBCLASS
    },
    {},
};

MODULE_DEVICE_TABLE( usb, global_ehub_usb_device_id );

static int ehub_xhci_get_configuration(struct usb_device *udev, int* configuration_index)
{
    int ret;

    *configuration_index = 0;
    ret = usb_control_msg(udev,
                          usb_rcvctrlpipe(udev, 0),
                          USB_REQ_GET_CONFIGURATION,
                          USB_DIR_IN | USB_RECIP_DEVICE,
                          0,
                          0,
                          ( u8* )configuration_index,
                          1,
                          USB_CTRL_GET_TIMEOUT);
    return ret;
}

static int ehub_xhci_set_configuration(struct usb_device *udev, int configuration_index)
{
    int ret;

    ret = usb_control_msg(udev,
                          usb_sndctrlpipe(udev, 0),
                          USB_REQ_SET_CONFIGURATION,
                          0,
                          configuration_index,
                          0,
                          NULL,
                          0,
                          USB_CTRL_SET_TIMEOUT);
    return ret;
}

/**
 * ehub_xhci_cache_work_expand - add new entries to the cache work queue.
 * @xhci: xhci structure
 * @entries: number of entries to add
 *
 * Create new cache work entries and add
 * them to xhci->cache_wq_list_free.
 *
 * MUST be called with SpinLockEmbeddedDoorbellWrite held!
 */
int
ehub_xhci_doorbell_expand(
    PDEVICE_CONTEXT DeviceContext,
    ulong entries,
    gfp_t flags
)
{
    int index = 0;
    PURB_CONTEXT urbContext;

    dev_dbg(dev_ctx_to_dev(DeviceContext), "expand doorbell entries: %lu\n", entries);
    for (index = 0; index < entries; index++) {
        urbContext = URB_Create(DeviceContext,
                                DeviceContext->UsbContext.UsbDevice,
                                DeviceContext->UsbContext.UsbPipeDoorbellOut,
                                sizeof(EMBEDDED_REGISTER_DATA_TRANSFER),
                                URB_CompletionRoutine_Doorbell,
                                NULL,
                                flags);
        if (NULL == urbContext)
            break;
        list_add_tail(&urbContext->list, &DeviceContext->doorbell_list_free);
    }

    return index;
}

static int
MODULE_UsbInterfaceConnect(
    struct usb_interface *interface,
    const struct usb_device_id *device_id
    )
{
    int status = 0;
    int interfaceNumber;
    int interfaceClass;
    int interfaceSubclass;
    int interfaceProtocol;
    PDEVICE_CONTEXT deviceContext;
    PURB_CONTEXT urbContext;
    int dataBufferLength;
    int indexOfUrbContext;
    int *currentConfiguration;

    FUNCTION_ENTRY;

    interfaceNumber = interface->cur_altsetting->desc.bInterfaceNumber;
    interfaceClass = interface->cur_altsetting->desc.bInterfaceClass;
    interfaceSubclass = interface->cur_altsetting->desc.bInterfaceSubClass;
    interfaceProtocol = interface->cur_altsetting->desc.bInterfaceProtocol;

    KOUT(STRING_MODULE_DESCRIPTION);

    dev_info(&interface->dev, "EHUB Connect InterfaceNumber : %d class=0x%0x subclass=0x%0x protocol=0x%0x \n",
               interfaceNumber,
               interfaceClass,
               interfaceSubclass,
               interfaceProtocol );

    //mutex_lock(&ehub_bus_lock);

    if ( EHUB_INTERFACE_NUMBER_BULK != interfaceNumber )
    {
        // Bypass this one.
        //
        goto Exit;
    }

    deviceContext = DEVICECONTEXT_Create(&interface->dev);
    if (NULL == deviceContext) {
        status = -ENOMEM;
        dev_err(&interface->dev, "ERROR: DeviceContext create failed %d\n", status);
        goto Exit;
    }

    dev_dbg(&interface->dev, "DeviceContext create success.\n" );

    // Setup interface data.
    //
    usb_set_intfdata( interface, deviceContext );
    deviceContext->InterfaceBackup = interface;

    deviceContext->UsbContext.UsbDevice = usb_get_dev(interface_to_usbdev( interface ));
    deviceContext->UsbContext.Device = &interface->dev;

    dev_dbg(&interface->dev, "SetConfiguration Check.\n" );

    currentConfiguration = kzalloc(sizeof( int ), GFP_KERNEL);

    status = ehub_xhci_get_configuration(deviceContext->UsbContext.UsbDevice,
                                         currentConfiguration);
    if (status < 0) {
        dev_err(dev_ctx_to_dev(deviceContext), "ERROR ERROR get_config failed with %d", status);
        kfree(currentConfiguration);
        usb_put_dev(deviceContext->UsbContext.UsbDevice);
        DEVICECONTEXT_Destroy(deviceContext);
        goto Exit;
    }
    if (0 == *currentConfiguration) {
        dev_dbg(&interface->dev, "SetConfiguration 1. \n");
        status = ehub_xhci_set_configuration(deviceContext->UsbContext.UsbDevice,
                                             EHUB_CONFIGURATION_INDEX_1);
        if (status < 0) {
            dev_err(dev_ctx_to_dev(deviceContext), "ERROR R set_config failed with %d", status);
            kfree(currentConfiguration);
            usb_put_dev(deviceContext->UsbContext.UsbDevice);
            DEVICECONTEXT_Destroy(deviceContext);
            goto Exit;
        }
    } else {
        dev_dbg(&interface->dev, "CurrentConfiguration : %d \n", *currentConfiguration);
    }

    kfree( currentConfiguration );

    deviceContext->UsbContext.UsbPipeBulkIn = usb_rcvbulkpipe( deviceContext->UsbContext.UsbDevice,
                                                               EHUB_ENDPOINT_NUMBER_BULK );
    ASSERT( 0 != deviceContext->UsbContext.UsbPipeBulkIn );

    deviceContext->UsbContext.UsbPipeBulkOut = usb_sndbulkpipe( deviceContext->UsbContext.UsbDevice,
                                                                EHUB_ENDPOINT_NUMBER_BULK );
    ASSERT( 0 != deviceContext->UsbContext.UsbPipeBulkOut );

    deviceContext->UsbContext.UsbPipeInterruptIn = usb_rcvintpipe( deviceContext->UsbContext.UsbDevice,
                                                                   EHUB_ENDPOINT_NUMBER_INTERRUPT );
    ASSERT( 0 != deviceContext->UsbContext.UsbPipeInterruptIn );

    deviceContext->UsbContext.UsbPipeInterruptOut = usb_sndintpipe( deviceContext->UsbContext.UsbDevice,
                                                                    EHUB_ENDPOINT_NUMBER_INTERRUPT );
    ASSERT( 0 != deviceContext->UsbContext.UsbPipeInterruptOut );

    deviceContext->UsbContext.UsbPipeIsochIn = usb_rcvisocpipe(deviceContext->UsbContext.UsbDevice,
                                                                  EHUB_ENDPOINT_NUMBER_ISOCH);
    ASSERT(0 != deviceContext->UsbContext.UsbPipeIsochIn);

    deviceContext->UsbContext.UsbPipeIsochOut = usb_sndisocpipe(deviceContext->UsbContext.UsbDevice,
                                                                   EHUB_ENDPOINT_NUMBER_ISOCH);
    ASSERT(0 != deviceContext->UsbContext.UsbPipeIsochOut);

    dev_dbg(&interface->dev, "UsbPipeBulkIn       : 0x%X \n", deviceContext->UsbContext.UsbPipeBulkIn );
    dev_dbg(&interface->dev, "UsbPipeBulkOut      : 0x%X \n", deviceContext->UsbContext.UsbPipeBulkOut );
    dev_dbg(&interface->dev, "UsbPipeInterruptIn  : 0x%X \n", deviceContext->UsbContext.UsbPipeInterruptIn );
    dev_dbg(&interface->dev, "UsbPipeInterruptOut : 0x%X \n", deviceContext->UsbContext.UsbPipeInterruptOut );
    dev_dbg(&interface->dev, "UsbPipeIsochIn      : 0x%X \n", deviceContext->UsbContext.UsbPipeIsochIn );
    dev_dbg(&interface->dev, "UsbPipeIsochOut     : 0x%X \n", deviceContext->UsbContext.UsbPipeIsochOut );
    dev_dbg(&interface->dev, "UsbPipeCacheOut     : 0x%X \n", deviceContext->UsbContext.UsbPipeCacheOut);
    dev_dbg(&interface->dev, "UsbPipeDoorbellOut  : 0x%X \n", deviceContext->UsbContext.UsbPipeDoorbellOut);

    /* Set endpoint used for cache writes here. */
    deviceContext->UsbContext.UsbPipeCacheOut = deviceContext->UsbContext.UsbPipeInterruptOut;
    //deviceContext->UsbContext.UsbPipeCacheOut = deviceContext->UsbContext.UsbPipeBulkOut;

    /* Set endpoint used for doorbell writes here. */
    deviceContext->UsbContext.UsbPipeDoorbellOut = deviceContext->UsbContext.UsbPipeInterruptOut;
    //deviceContext->UsbContext.UsbPipeDoorbellOut = deviceContext->UsbContext.UsbPipeBulkOut;

    // URB_CONTEXT for dedicate embedded register read/write/cache write.
    //
    urbContext = URB_Create(deviceContext,
                            deviceContext->UsbContext.UsbDevice,
                            deviceContext->UsbContext.UsbPipeBulkOut,
                            sizeof(EMBEDDED_REGISTER_DATA_TRANSFER),
                            URB_CompletionRoutine_Simple,
                            NULL,
                            GFP_KERNEL);
    ASSERT( NULL != urbContext );
    deviceContext->UrbContextEmbeddedRegisterRead = urbContext;

    urbContext = URB_Create(deviceContext,
                            deviceContext->UsbContext.UsbDevice,
                            deviceContext->UsbContext.UsbPipeBulkOut,
                            sizeof(EMBEDDED_REGISTER_DATA_TRANSFER),
                            URB_CompletionRoutine_Simple,
                            NULL,
                            GFP_KERNEL);
    ASSERT( NULL != urbContext );
    deviceContext->UrbContextEmbeddedRegisterWrite = urbContext;

    dataBufferLength = sizeof( EMBEDDED_REGISTER_DATA_TRANSFER );

    ehub_xhci_doorbell_expand(deviceContext, NUMBER_OF_MESSAGE_DOORBELL, GFP_KERNEL);

    // Default index is zero.
    //
    dataBufferLength = MESSAGE_DATA_BUFFER_SIZE_BULK;
    for ( indexOfUrbContext = 0; indexOfUrbContext < NUMBER_OF_MESSAGE_BULK; indexOfUrbContext++ )
    {
        urbContext = URB_Create(deviceContext,
                                deviceContext->UsbContext.UsbDevice,
                                deviceContext->UsbContext.UsbPipeBulkOut,
                                dataBufferLength,
                                URB_CompletionRoutine_Simple,
                                NULL,
                                GFP_KERNEL);
        ASSERT( NULL != urbContext );
        deviceContext->UrbContextEmbeddedMemoryReadCompletion[ indexOfUrbContext ] = urbContext;
    }
    // Default index is zero.
    //
    deviceContext->CurrentEmbeddedMemoryReadCompletionUrbContextIndex = 0;

    status = USB_InterfaceCreateBulk( deviceContext );
    if (status < 0) {
        dev_err(dev_ctx_to_dev(deviceContext), "ERROR USB_InterfaceCreateBulk fail %d\n",status );
        usb_put_dev(deviceContext->UsbContext.UsbDevice);
        DEVICECONTEXT_Destroy(deviceContext);
        goto Exit;
    } else {
        dev_info(dev_ctx_to_dev(deviceContext), "USB_InterfaceCreateBulk success.\n");
    }

    status = USB_InterfaceCreateInterrupt( deviceContext );
    if (status < 0) {
        dev_err(dev_ctx_to_dev(deviceContext), "ERROR USB_InterfaceCreateInterrupt fail %d\n",status );
        usb_put_dev(deviceContext->UsbContext.UsbDevice);
        DEVICECONTEXT_Destroy(deviceContext);
        goto Exit;
    } else {
        dev_info(dev_ctx_to_dev(deviceContext), "USB_InterfaceCreateInterrupt success.\n" );
    }

    // Add embedded host driver.
    //
    status = ehub_xhci_add( deviceContext );
    if (status < 0) {
        dev_err(dev_ctx_to_dev(deviceContext), "ERROR ehub_xhci_add fail %d", status );
        usb_put_dev(deviceContext->UsbContext.UsbDevice);
        DEVICECONTEXT_Destroy(deviceContext);
        goto Exit;
    }

#ifdef EHUB_ISOCH_ENABLE
    status = USB_InterfaceCreateIsoch(deviceContext);
    if (status < 0) {
        dev_err(dev_ctx_to_dev(deviceContext), "ERROR USB_InterfaceCreateIsoch fail %d\n", status);
        usb_put_dev(deviceContext->UsbContext.UsbDevice);
        DEVICECONTEXT_Destroy(deviceContext);
        goto Exit;
    } else {
        dev_info(dev_ctx_to_dev(deviceContext), "USB_InterfaceCreateIsoch success.\n");
    }
#endif /* EHUB_ISOCH_ENABLE */

    dev_dbg(&interface->dev, "\n");

Exit:

    //mutex_unlock(&ehub_bus_lock);

    FUNCTION_LEAVE;

    return status;
}

static void
MODULE_UsbInterfaceDisconnect(
    struct usb_interface *interface
    )
{
    int status = 0;
    int interfaceNumber;
    int interfaceClass;
    int interfaceSubclass;
    int interfaceProtocol;
    PDEVICE_CONTEXT deviceContext;
    int indexOfUrbContext;
    URB_CONTEXT *q, *urbContext;
    struct device *dev;
    struct usb_hcd  *hcd;


    FUNCTION_ENTRY;

    interfaceNumber = interface->cur_altsetting->desc.bInterfaceNumber;
    interfaceClass = interface->cur_altsetting->desc.bInterfaceClass;
    interfaceSubclass = interface->cur_altsetting->desc.bInterfaceSubClass;
    interfaceProtocol = interface->cur_altsetting->desc.bInterfaceProtocol;

    dev_info(&interface->dev, "EHUB Disconnect InterfaceNumber : %d class=0x%0x subclass=0x%0x protocol=0x%0x \n",
               interfaceNumber,
               interfaceClass,
               interfaceSubclass,
               interfaceProtocol );

    if ( EHUB_INTERFACE_NUMBER_BULK != interfaceNumber )
    {
        // Bypass this one.
        //
        goto Exit;
    }

    deviceContext = ( PDEVICE_CONTEXT )usb_get_intfdata( interface );
    ASSERT ( NULL != deviceContext );

    dev = &deviceContext->UsbContext.UsbDevice->dev;
    hcd = dev_get_drvdata(dev);

    usb_hc_died(hcd);

    if (deviceContext->InterfaceBackup->condition != USB_INTERFACE_UNBINDING) {
        dev_err(dev_ctx_to_dev(deviceContext), "ERROR: Interface not in UNBINDING\n" );
    }

    ehub_xhci_remove( deviceContext );

    if (deviceContext->WorkItemQueue) {
        destroy_workqueue(deviceContext->WorkItemQueue);
        deviceContext->WorkItemQueue = NULL;
    }

#ifdef EHUB_ISOCH_ENABLE
    status = USB_InterfaceDestroyIsoch(deviceContext);
    if (status < 0) {
        dev_warn(dev_ctx_to_dev(deviceContext), "USB_InterfaceDestroyIsoch fail! %d\n", status);
    }
#endif /* EHUB_ISOCH_ENABLE */

    status = USB_InterfaceDestroyBulk( deviceContext );
    if (status < 0) {
        dev_dbg(&interface->dev, "USB_InterfaceDestroyBulk fail! %d\n", status );
    }

    status = USB_InterfaceDestroyInterrupt( deviceContext );
    if (status < 0) {
        dev_dbg(&interface->dev, "USB_InterfaceDestroyInterrupt fail! %d\n", status );
    }

    if (USB_STATE_NOTATTACHED == deviceContext->UsbContext.UsbDevice->state)
    {
        status = -ENODEV;
        dev_dbg(dev_ctx_to_dev(deviceContext), "ERROR FL6000 is not attached!\n" );
    }
    else
    {
        int *currentConfiguration;

        dev_dbg(&interface->dev, "SetConfiguration Check.\n" );

        currentConfiguration = kzalloc(sizeof( int ), GFP_KERNEL);

        status = ehub_xhci_get_configuration(deviceContext->UsbContext.UsbDevice,
                                             currentConfiguration);
        if (unlikely(status < 0)) {
            dev_err(dev_ctx_to_dev(deviceContext), "ERROR get_config failed with %d", status);
            kfree(currentConfiguration);
            goto Cleanup;
        }

        if ( 1 == *currentConfiguration )
        {
            dev_dbg(&interface->dev, "SetConfiguration 0. \n");
            status = ehub_xhci_set_configuration(deviceContext->UsbContext.UsbDevice,
                                                 EHUB_CONFIGURATION_INDEX_0);
            if (status < 0) {
                dev_err(dev_ctx_to_dev(deviceContext), "ERROR set_config failed with %d", status);
                kfree(currentConfiguration);
                goto Cleanup;
            }
        }
        else
        {
            dev_dbg(&interface->dev, "CurrentConfiguration : %d \n", *currentConfiguration );
        }

        kfree( currentConfiguration );
    }

Cleanup:

    list_for_each_entry_safe(urbContext, q, &deviceContext->doorbell_list_busy, list)
    {
        if (NULL != urbContext) {
            list_del_init(&urbContext->list);
            if (NULL != urbContext)
                URB_Destroy(urbContext);
        }
    }

    list_for_each_entry_safe(urbContext, q, &deviceContext->doorbell_list_free, list)
    {
        if (NULL != urbContext) {
            list_del_init(&urbContext->list);
            if (NULL != urbContext)
                URB_Destroy(urbContext);
        }
    }

    for ( indexOfUrbContext = 0; indexOfUrbContext < NUMBER_OF_MESSAGE_BULK; indexOfUrbContext++ )
    {
        if ( NULL != deviceContext->UrbContextEmbeddedMemoryReadCompletion[ indexOfUrbContext ] )
        {
            URB_Destroy( deviceContext->UrbContextEmbeddedMemoryReadCompletion[ indexOfUrbContext ] );
            deviceContext->UrbContextEmbeddedMemoryReadCompletion[ indexOfUrbContext ] = NULL;
        }
    }

    if ( NULL != deviceContext->UrbContextEmbeddedRegisterRead )
    {
        URB_Destroy( deviceContext->UrbContextEmbeddedRegisterRead );
        deviceContext->UrbContextEmbeddedRegisterRead = NULL;
    }

    if ( NULL != deviceContext->UrbContextEmbeddedRegisterWrite )
    {
        URB_Destroy( deviceContext->UrbContextEmbeddedRegisterWrite );
        deviceContext->UrbContextEmbeddedRegisterWrite = NULL;
    }

    usb_put_dev(deviceContext->UsbContext.UsbDevice);
    DEVICECONTEXT_Destroy( deviceContext );
    deviceContext = NULL;

Exit:

    //mutex_unlock(&ehub_bus_lock);

    FUNCTION_LEAVE;
}

static int
MODULE_Suspend(
    struct usb_interface *interface,
    pm_message_t message
    )
{
    int status = 0;
    int interfaceNumber;
    int interfaceClass;
    int interfaceSubclass;
    int interfaceProtocol;
    PDEVICE_CONTEXT deviceContext;

    FUNCTION_ENTRY;

    interfaceNumber = interface->cur_altsetting->desc.bInterfaceNumber;
    interfaceClass = interface->cur_altsetting->desc.bInterfaceClass;
    interfaceSubclass = interface->cur_altsetting->desc.bInterfaceSubClass;
    interfaceProtocol = interface->cur_altsetting->desc.bInterfaceProtocol;

    dev_dbg(&interface->dev, "InterfaceNumber : %d class=0x%0x subclass=0x%0x protocol=0x%0x \n",
               interfaceNumber,
               interfaceClass,
               interfaceSubclass,
               interfaceProtocol );

    if ( EHUB_INTERFACE_NUMBER_BULK != interfaceNumber )
    {
        // Bypass this one.
        //
        goto Exit;
    }

    deviceContext = usb_get_intfdata( interface );

    status = DEVICECONTEXT_ErrorCheck( deviceContext );
    if (status < 0)
    {
        goto Exit;
    }

    status = USB_InterfaceSuspendBulk( deviceContext );
    if (status < 0)
    {
        dev_err(dev_ctx_to_dev(deviceContext), "ERROR USB_InterfaceSuspendBulk fail! %d\n", status );
        goto Exit;
    }

#ifdef EHUB_ISOCH_ENABLE
    status = USB_InterfaceSuspendIsoch( deviceContext );
    if (status < 0)
    {
        dev_err(dev_ctx_to_dev(deviceContext), "ERROR USB_InterfaceSuspendIsoch fail! %d\n", status );
        goto Exit;
    }
#endif /* EHUB_ISOCH_ENABLE */

    status = USB_InterfaceSuspendInterrupt( deviceContext );
    if (status < 0)
    {
        dev_err(dev_ctx_to_dev(deviceContext), "ERROR USB_InterfaceSuspendInterrupt fail! %d\n", status );
        goto Exit;
    }

Exit:

    FUNCTION_LEAVE;

    return status;
}

static int
MODULE_Resume(
    struct usb_interface *interface
    )
{
    int status = 0;
    int interfaceNumber;
    int interfaceClass;
    int interfaceSubclass;
    int interfaceProtocol;
    PDEVICE_CONTEXT deviceContext;

    FUNCTION_ENTRY;

    interfaceNumber = interface->cur_altsetting->desc.bInterfaceNumber;
    interfaceClass = interface->cur_altsetting->desc.bInterfaceClass;
    interfaceSubclass = interface->cur_altsetting->desc.bInterfaceSubClass;
    interfaceProtocol = interface->cur_altsetting->desc.bInterfaceProtocol;

    dev_dbg(&interface->dev, "InterfaceNumber : %d class=0x%0x subclass=0x%0x protocol=0x%0x \n",
               interfaceNumber,
               interfaceClass,
               interfaceSubclass,
               interfaceProtocol );

    if ( EHUB_INTERFACE_NUMBER_BULK != interfaceNumber )
    {
        // Bypass this one.
        //
        goto Exit;
    }

    deviceContext = usb_get_intfdata( interface );

    status = DEVICECONTEXT_ErrorCheck( deviceContext );
    if (status < 0)
    {
        goto Exit;
    }

    status = USB_InterfaceResumeBulk( deviceContext );
    if (status < 0)
    {
        dev_err(dev_ctx_to_dev(deviceContext), "ERROR USB_InterfaceResumeBulk fail! %d\n", status );
        goto Exit;
    }

#ifdef EHUB_ISOCH_ENABLE
    status = USB_InterfaceResumeIsoch( deviceContext );
    if (status < 0)
    {
        dev_err(dev_ctx_to_dev(deviceContext), "ERROR USB_InterfaceResumeIsoch fail! %d\n", status );
        goto Exit;
    }
#endif /* EHUB_ISOCH_ENABLE */

    status = USB_InterfaceResumeInterrupt( deviceContext );
    if (status < 0)
    {
        dev_err(dev_ctx_to_dev(deviceContext), "ERROR USB_InterfaceResumeInterrupt fail! %d\n", status );
        goto Exit;
    }

Exit:

    FUNCTION_LEAVE;

    return status;
}

static int
MODULE_ResetResume(
    struct usb_interface *interface
    )
{
    int status = 0;
    int interfaceNumber;
    int interfaceClass;
    int interfaceSubclass;
    int interfaceProtocol;
    PDEVICE_CONTEXT deviceContext;

    FUNCTION_ENTRY;

    interfaceNumber = interface->cur_altsetting->desc.bInterfaceNumber;
    interfaceClass = interface->cur_altsetting->desc.bInterfaceClass;
    interfaceSubclass = interface->cur_altsetting->desc.bInterfaceSubClass;
    interfaceProtocol = interface->cur_altsetting->desc.bInterfaceProtocol;

    dev_dbg(&interface->dev, "InterfaceNumber : %d class=0x%0x subclass=0x%0x protocol=0x%0x \n",
               interfaceNumber,
               interfaceClass,
               interfaceSubclass,
               interfaceProtocol );

    if ( EHUB_INTERFACE_NUMBER_BULK != interfaceNumber )
    {
        // Bypass this one.
        //
        goto Exit;
    }

    deviceContext = usb_get_intfdata( interface );

    status = DEVICECONTEXT_ErrorCheck( deviceContext );
    if (status < 0)
    {
        goto Exit;
    }

    status = USB_InterfaceResumeBulk( deviceContext );
    if (status < 0)
    {
        dev_err(dev_ctx_to_dev(deviceContext), "ERROR USB_InterfaceResumeBulk fail! %d\n", status );
        goto Exit;
    }

#ifdef EHUB_ISOCH_ENABLE
    status = USB_InterfaceResumeIsoch( deviceContext );
    if (status < 0)
    {
        dev_err(dev_ctx_to_dev(deviceContext), "ERROR USB_InterfaceResumeIsoch fail! %d\n", status );
        goto Exit;
    }
#endif /* EHUB_ISOCH_ENABLE */

    status = USB_InterfaceResumeInterrupt( deviceContext );
    if (status < 0)
    {
        dev_err(dev_ctx_to_dev(deviceContext), "ERROR USB_InterfaceResumeInterrupt fail! %d\n", status );
        goto Exit;
    }

Exit:

    FUNCTION_LEAVE;

    return status;
}

static struct usb_driver global_ehub_usb_driver =
{
    .name           = "ehub",
    .id_table       = global_ehub_usb_device_id,
    .probe          = MODULE_UsbInterfaceConnect,
    .disconnect     = MODULE_UsbInterfaceDisconnect,
    .suspend        = MODULE_Suspend,
    .resume         = MODULE_Resume,
    .reset_resume   = MODULE_ResetResume,
    .supports_autosuspend = 1,
};

static int __init
MODULE_Init(void)
{
    int status;

    KOUT( STRING_MODULE_DESCRIPTION );

#ifdef EHUB_DEBUG_ENABLE
    KOUT( " DEBUG MODE.\n" );
#endif

    status = usb_register( &global_ehub_usb_driver );
    if ( status < 0 )
    {
        printk(KERN_ERR "%s: ERROR usb_register failed. Error number %d\n", __FUNCTION__, status );
    }

    return status;
}

static void __exit
MODULE_Exit(void)
{
    usb_deregister( &global_ehub_usb_driver );
}

module_init( MODULE_Init );
module_exit( MODULE_Exit );
