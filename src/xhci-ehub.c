/*
 * Fresco Logic FL6000 F-One Controller Driver
 *
 * Copyright (C) 2008 Intel Corp.
 * Modifications Copyright (C) 2014-2017 Fresco Logic, Inc.
 *
 * Author: Sarah Sharp
 * Modified from Linux xHCI host controller driver.
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
 */

#include <linux/slab.h>
#include <linux/pci.h>

#include "xhci.h"
#include "ehub_device_context.h"
#include "ehub_embedded_register.h"
#include "ehub_usb.h"
#include "ehub_urb.h"
#include "ehub-xhci-trace.h"

static const struct hc_driver ehub_xhci_xhci_driver = {
    .description        =   "ehub-xhci-hcd",
    .product_desc       =   "Embedded xHCI Host Controller",
    .hcd_priv_size      =   sizeof(struct xhci_hcd *),

    /*
     * generic hardware linkage
     */
    .irq                =   NULL,
    .flags              =   HCD_MEMORY | HCD_USB3,

    /*
     * basic lifecycle operations
     */
    /* TODO: EHUB Set from xhci_init_driver like stock version */
    .reset              =   ehub_xhci_reset_device,
    .start              =   ehub_xhci_run,
    .stop               =   ehub_xhci_stop,
    .shutdown           =   ehub_xhci_shutdown,

    /*
     * managing i/o requests and associated device resources
     */
    .urb_enqueue        =   ehub_xhci_urb_enqueue,
    .urb_dequeue        =   ehub_xhci_urb_dequeue,

    .map_urb_for_dma    =   ehub_xhci_map_urb_for_dma,
    .unmap_urb_for_dma  =   ehub_xhci_unmap_urb_for_dma,

    .alloc_dev          =   ehub_xhci_alloc_dev,
    .free_dev           =   ehub_xhci_free_dev,
    .alloc_streams      =   NULL,
    .free_streams       =   NULL,
    .add_endpoint       =   ehub_xhci_add_endpoint,
    .drop_endpoint      =   ehub_xhci_drop_endpoint,
    .endpoint_reset     =   ehub_xhci_endpoint_reset,
    .check_bandwidth    =   ehub_xhci_check_bandwidth,
    .reset_bandwidth    =   ehub_xhci_reset_bandwidth,
    .address_device     =   ehub_xhci_address_device,
    .enable_device      =   ehub_xhci_enable_device,
    .update_hub_device  =   ehub_xhci_update_hub_device,
    .reset_device       =   ehub_xhci_discover_or_reset_device,

    /*
     * scheduling support
     */
    .get_frame_number   =   ehub_xhci_get_frame,

    /*
     * root hub support
     */
    .hub_control        =   ehub_xhci_hub_control,
    .hub_status_data    =   ehub_xhci_hub_status_data,
    .bus_suspend        =   ehub_xhci_bus_suspend,
    .bus_resume         =   ehub_xhci_bus_resume,

    /*
     * call back when device connected and addressed
     */
    .update_device      =   ehub_xhci_update_device,
    .set_usb2_hw_lpm    =   ehub_xhci_set_usb2_hardware_lpm,
    .enable_usb3_lpm_timeout =  ehub_xhci_enable_usb3_lpm_timeout,
    .disable_usb3_lpm_timeout = ehub_xhci_disable_usb3_lpm_timeout,
    .find_raw_port_number = ehub_xhci_find_raw_port_number,
};

unsigned int
ehub_xhci_readl(
    const struct xhci_hcd *xhci,
    __le32 *regs
    )
{
    PDEVICE_CONTEXT deviceContext;
    u32 rdata;
    int status = 0;

    deviceContext = xhci->DeviceContext;

    status = DEVICECONTEXT_ErrorCheck( deviceContext );
    if (status < 0)
    {
        /* TODO: EHUB set device status to dying somehow? */
        rdata = ~(0);
        goto Exit;
    }

    status = EMBEDDED_REGISTER_Read( deviceContext,
                                     ( unsigned long )regs,
                                     &rdata );
    if (status < 0)
    {
        rdata = ~(0);
        goto Exit;
    }

Exit:

    return rdata;
}

void
ehub_xhci_writel(
    struct xhci_hcd *xhci,
    const unsigned int val,
    __le32 *regs
    )
{
    PDEVICE_CONTEXT deviceContext;
    int status;

    deviceContext = xhci->DeviceContext;

    status = DEVICECONTEXT_ErrorCheck( deviceContext );
    if (status < 0)
    {
        goto Exit;
    }

    status = EMBEDDED_REGISTER_Write( deviceContext,
                                      ( unsigned long )regs,
                                      ( u32 * )&val );
    if (status < 0)
    {
        goto Exit;
    }

Exit:
    ;
}

void
ehub_xhci_writel_doorbell(
    struct xhci_hcd *xhci,
    const unsigned int val,
    __le32 *regs
    )
{
    PDEVICE_CONTEXT deviceContext;
    int status;

    deviceContext = xhci->DeviceContext;

    status = DEVICECONTEXT_ErrorCheck( deviceContext );
    if (status < 0)
    {
        goto Exit;
    }

    status = EMBEDDED_REGISTER_Write_Doorbell( deviceContext,
                                               ( unsigned long )regs,
                                               ( u32 * )&val );
    if (status < 0)
    {
        goto Exit;
    }

Exit:
    ;
}

/* Code adapted from xhci_gen_setup */
int ehub_xhci_reset_device(struct usb_hcd *hcd)
{
    struct xhci_hcd *xhci;
    int retval;

    FUNCTION_ENTRY;

    xhci = hcd_to_xhci( hcd );
    ASSERT( NULL != xhci );

    xhci_dbg(xhci, "hcd=0x%p\n", hcd );
    xhci_dbg(xhci, "xhci=0x%p\n", xhci );
    xhci_dbg(xhci, "xhci->DeviceContext=0x%p\n", xhci->DeviceContext );

    /* Accept arbitrarily long scatter-gather lists */
    hcd->self.sg_tablesize = ~0;

    /* Mark the first roothub as being USB 2.0.
     * The xHCI driver will register the USB 3.0 roothub.
     */
    hcd->speed = HCD_USB2;
    hcd->self.root_hub->speed = USB_SPEED_HIGH;
    /*
     * USB 2.0 roothub under xHCI has an integrated TT,
     * (rate matching hub) as opposed to having an OHCI/UHCI
     * companion controller.
     */
    hcd->has_tt = 1;

    mutex_init(&xhci->mutex);

    xhci->cap_regs = hcd->regs;

    /* Cache read-only capability registers */
    xhci->hc_capbase    = xhci_readl(xhci, &xhci->cap_regs->hc_capbase);
    xhci->hci_version   = HC_VERSION(xhci->hc_capbase);
    xhci->hcs_params1   = xhci_readl(xhci, &xhci->cap_regs->hcs_params1);
    xhci->hcs_params2   = xhci_readl(xhci, &xhci->cap_regs->hcs_params2);
    xhci->hcs_params3   = xhci_readl(xhci, &xhci->cap_regs->hcs_params3);
    xhci->hcc_params    = xhci_readl(xhci, &xhci->cap_regs->hcc_params);
    xhci->db_off        = xhci_readl(xhci, &xhci->cap_regs->db_off);
    xhci->run_regs_off  = xhci_readl(xhci, &xhci->cap_regs->run_regs_off);

    xhci->op_regs = hcd->regs +
        HC_LENGTH(xhci->hc_capbase);
    xhci->run_regs = hcd->regs +
        (xhci->run_regs_off & RTSOFF_MASK);

    /* In xhci controllers which follow xhci 1.0 spec gives a spurious
     * success event after a short transfer. This quirk will ignore such
     * spurious event.
     */
    if (xhci->hci_version > 0x96)
        xhci->quirks |= XHCI_SPURIOUS_SUCCESS;

    /* Make sure the HC is halted. */
    retval = ehub_xhci_halt(xhci);
    if (retval)
    {
        xhci_err(xhci, "ERROR ehub_xhci_halt fail!" );
        goto error;
    }

    xhci_dbg(xhci, "Resetting HCD\n");
    /* Reset the internal HC memory state and registers. */
    retval = ehub_xhci_reset(xhci);
    if (retval)
    {
        xhci_err(xhci, "ERROR ehub_xhci_reset fail!" );
        goto error;
    }
    xhci_dbg(xhci, "Reset complete\n");

    /* Set dma_mask and coherent_dma_mask to 64-bits,
     * if xHC supports 64-bit addressing */
    if (HCC_64BIT_ADDR(xhci->hcc_params) &&
            !dma_set_mask(hcd->self.controller, DMA_BIT_MASK(64))) {
        xhci_dbg(xhci, "Enabling 64-bit DMA addresses. hcc=0x%0X\n",xhci->hcc_params);
    } else {
        xhci_dbg(xhci, "32 bit addressing hcc=0x%0X\n", xhci->hcc_params);

    }

    xhci_dbg(xhci, "Calling HCD init\n");
    /* Initialize HCD and host controller data structures. */
    retval = ehub_xhci_init(hcd);
    if (retval)
    {
        xhci_err(xhci, "ERROR: HCD init failed\n");
        goto error;
    }
    else
    {
        PDEVICE_CONTEXT deviceContext;

        deviceContext = xhci->DeviceContext;

        // We need this flag to prevent early interrupt for port status change.
        //
        deviceContext->ControlFlags.IsEhubXhciInitReady = 1;
        xhci_dbg(xhci, "EhubXhciInit Ready.\n");
    }

    xhci_info(xhci, "hcc params 0x%08x hci version 0x%x quirks 0x%08x\n",
          xhci->hcc_params, xhci->hci_version, xhci->quirks);

    /* Capture an initial value of the microframe count to base all future *
     * frame time values from. */
    xhci_readl(xhci, &xhci->run_regs->microframe_index);

    FUNCTION_LEAVE;
    return 0;
error:
    FUNCTION_LEAVE;
    return retval;
}

/* Code adapted from xhci_plat_probe */
int ehub_xhci_add(
    PDEVICE_CONTEXT DeviceContext
    )
{
    struct device *dev;
    const struct hc_driver *driver;
    struct usb_hcd *hcd;
    struct xhci_hcd *xhci;
    int ret;

    FUNCTION_ENTRY;

    ret = 0;

    if ( usb_disabled() )
    {
        dev_err(dev_ctx_to_dev(DeviceContext), "ERROR Usb disabled!" );
        ret = -ENODEV;
        goto Exit;
    }

    dev = &DeviceContext->UsbContext.UsbDevice->dev;

    driver = &ehub_xhci_xhci_driver;

    dev_dbg(dev_ctx_to_dev(DeviceContext), "usb_create_hcd...\n" );
    hcd = usb_create_hcd( driver,
                          dev,
                          dev_name( dev ) );
    if ( !hcd )
    {
        dev_err(dev_ctx_to_dev(DeviceContext), "ERROR usb_create_hcd failed!" );
        ret = -ENOMEM;
        goto Exit;
    }
    dev_dbg(dev_ctx_to_dev(DeviceContext), "usb_create_hcd...done \n" );

    xhci = kzalloc(sizeof(struct xhci_hcd), GFP_KERNEL);
    if (!xhci)
        return -ENOMEM;

    memset( xhci, 0, sizeof( struct xhci_hcd ) );

    dev_dbg(dev, "xhci=0x%p\n", xhci );
    xhci->DeviceContext = DeviceContext;
    dev_dbg(dev, "xhci->DeviceContext=0x%p\n", xhci->DeviceContext );
    *((struct xhci_hcd **) hcd->hcd_priv) = xhci;
    xhci->main_hcd = hcd;
    DeviceContext->UsbContext.xhci_hcd = ( void* )xhci;
    dev_dbg(dev, "DeviceContext->UsbContext.xhci_hcd=0x%p\n", DeviceContext->UsbContext.xhci_hcd );

    // We don't have real hardware mmio and irq of embedded host.
    //
    hcd->rsrc_start = 0;
    hcd->rsrc_len = 0;
    hcd->regs = 0;

#ifdef USE_TRB_CACHE_MODE
    ret = ehub_xhci_cache_create(xhci);
    if (ret) {
        dev_err(dev_ctx_to_dev(DeviceContext), "ERROR creating cache! %d", ret);
        kfree(xhci);
        goto Exit;
    }
#endif

    dev_dbg(dev_ctx_to_dev(DeviceContext), "usb_add_hcd...\n" );
    ret = usb_add_hcd( hcd,
                       0,
                       0 );
    if ( ret )
    {
        dev_err(dev_ctx_to_dev(DeviceContext), "ERROR usb_add_hcd failed! %d", ret);
        ehub_xhci_cache_destroy(xhci);
        kfree(xhci);
        goto Exit;
    }
    dev_dbg(dev_ctx_to_dev(DeviceContext), "usb_add_hcd...done \n" );

    hcd->rh_pollable = false;

Exit:

    FUNCTION_LEAVE;

    return ret;
}

/* Code adapted from xhci_plat_remove */
int
ehub_xhci_remove(
    PDEVICE_CONTEXT DeviceContext
    )
{
    struct device *dev;
    struct usb_hcd  *hcd;
    struct xhci_hcd *xhci;

    FUNCTION_ENTRY;

    dev = &DeviceContext->UsbContext.UsbDevice->dev;
    hcd = dev_get_drvdata( dev );
    xhci = hcd_to_xhci( hcd );

    dev_dbg(dev, "dev=0x%p\n", dev );
    dev_dbg(dev, "hcd=0x%p\n", hcd );
    dev_dbg(dev, "xhci=0x%p\n", xhci );

    hcd->rh_pollable = 0;
    dev_warn(dev, "Calling usb_remove_hcd\n");
    usb_remove_hcd( hcd );

    dev_warn(dev, "Calling usb_put_hcd\n");
    usb_put_hcd( hcd );
#ifdef USE_TRB_CACHE_MODE
    dev_warn(dev, "Calling ehub_xhci_cache_destroy\n");
    ehub_xhci_cache_destroy( xhci );
#endif
    dev_warn(dev, "Calling kfree( xhci )\n");
    kfree( xhci );

    DeviceContext->UsbContext.IsUsbHcdInvalid = 1;

    FUNCTION_LEAVE;

    return 0;
}

#ifdef USE_TRB_CACHE_MODE
static void ehub_delayed_cache_write(struct work_struct *work)
{
    struct xhci_hcd *xhci;
    unsigned long flags;
    unsigned long total_length = 0;
    struct cache_write_context *ehub_cache_work, *temp_context, *next;
    LIST_HEAD(tmp_list);

    temp_context = container_of(work,
                                 struct cache_write_context,
                                 work);

    xhci = dev_ctx_to_xhci(temp_context->DeviceContext);

    spin_lock_irqsave( &xhci->cache_list_lock, flags );

    /* Cut all the pending entries from the list and put them in tmp_list so
     * there is no confusion about what we need to do.
     */
    list_cut_position(&tmp_list, &xhci->cache_queue_used, xhci->cache_queue_used.prev);

    spin_unlock_irqrestore(&xhci->cache_list_lock, flags);

    list_for_each_entry_safe(ehub_cache_work, next, &tmp_list, list) {

        dev_dbg(dev_ctx_to_dev(temp_context->DeviceContext),
                 "d_cw cw=0x%p CA=0x%0X Buf=0x%p len=0x%0X DB=%d SL=%d EP=%d Stream=0x%X\n",
                 ehub_cache_work,
                 ehub_cache_work->CacheAddress,
                 ehub_cache_work->DataBuffer,
                 ehub_cache_work->DataBufferLength,
                 ehub_cache_work->ring_doorbell,
                 ehub_cache_work->slot_id,
                 ehub_cache_work->ep_index,
                 ehub_cache_work->stream_id);
        total_length += sizeof(EMBEDDED_CACHE_TRANSFER) + ehub_cache_work->DataBufferLength;

        if (total_length > MESSAGE_DATA_BUFFER_SIZE_CACHE)
            break;

        list_del_init(&ehub_cache_work->list);

        if (!EMBEDDED_CACHE_Write(ehub_cache_work->DeviceContext,
                                  ehub_cache_work->UrbContext,
                                  ehub_cache_work->CacheAddress,
                                  ehub_cache_work->DataBuffer,
                                  ehub_cache_work->DataBufferLength,
                                  ehub_cache_work->TrbCycleState)) {
        }
    }
}

int
ehub_queue_cache_write(
    PDEVICE_CONTEXT DeviceContext,
    u32 CacheAddress,
    u32* DataBuffer,
    u32 DataBufferLength,
    int TrbCycleState,
    bool ring_doorbell,
    unsigned int slot_id,
    unsigned int ep_index,
    unsigned int stream_id
)
{
    struct xhci_hcd *xhci;
    unsigned long flags;
    struct cache_write_context *ehub_cache_work;
    int new_entries = 1;
    int status = 0;

    xhci = dev_ctx_to_xhci(DeviceContext);

    status = DEVICECONTEXT_ErrorCheck(DeviceContext);
    if (status < 0)
        return status;

    spin_lock_irqsave( &xhci->cache_list_lock, flags );

    if (list_empty(&xhci->cache_queue_free))
        new_entries = ehub_xhci_cache_work_expand(xhci, 4, GFP_ATOMIC);

    if (new_entries < 1) {
        ASSERT(false);
        spin_unlock_irqrestore(&xhci->cache_list_lock, flags);
        return -ENOBUFS;
    }

    ehub_cache_work = list_first_entry(&xhci->cache_queue_free,
                                       struct cache_write_context,
                                       list);

    ehub_cache_work->DeviceContext = DeviceContext;
    ehub_cache_work->CacheAddress = CacheAddress;
    ehub_cache_work->DataBuffer = DataBuffer;
    ehub_cache_work->DataBufferLength = DataBufferLength;
    ehub_cache_work->TrbCycleState = TrbCycleState;
    ehub_cache_work->ring_doorbell = ring_doorbell;
    ehub_cache_work->slot_id = slot_id;
    ehub_cache_work->ep_index = ep_index;
    ehub_cache_work->stream_id = stream_id;

    list_move_tail(&ehub_cache_work->list,
                   &xhci->cache_queue_used);

    dev_dbg(dev_ctx_to_dev(DeviceContext), "q_cw from %ps cw=0x%p CA=0x%0X Buf=0x%p len=0x%0X DB=%d SL=%d EP=%d Stream=0x%0X\n",
             __builtin_return_address(0),
             ehub_cache_work,
             ehub_cache_work->CacheAddress,
             ehub_cache_work->DataBuffer,
             ehub_cache_work->DataBufferLength,
             ehub_cache_work->ring_doorbell,
             ehub_cache_work->slot_id,
             ehub_cache_work->ep_index,
             ehub_cache_work->stream_id);

    spin_unlock_irqrestore( &xhci->cache_list_lock, flags );

#ifdef USE_DELAYED_CACHE_MODE
    queue_work(xhci->ehub_cache_wq, &xhci->ehub_cache_work.work);
#else /* ! USE_DELAYED_CACHE_MODE */
    status = EMBEDDED_CACHE_Write(ehub_cache_work->DeviceContext,
                                  ehub_cache_work->UrbContext,
                                  ehub_cache_work->CacheAddress,
                                  ehub_cache_work->DataBuffer,
                                  ehub_cache_work->DataBufferLength,
                                  ehub_cache_work->TrbCycleState);
    if (status < 0)
        dev_err(dev_ctx_to_dev(DeviceContext), "ERROR: Cache Write failed %d\n", status);
#endif /* ! USE_DELAYED_CACHE_MODE */

    return status;
}

void
URB_CompletionRoutine_Cache(
    struct urb *Urb
)
{
    //struct cache_write_context *ehub_cache_work = (struct cache_write_context *)(Urb->context);
    struct cache_write_context *ehub_cache_work = Urb->context;
    struct xhci_hcd *xhci = dev_ctx_to_xhci(ehub_cache_work->DeviceContext);
    unsigned long flags;

    if (DEVICECONTEXT_ErrorCheck(ehub_cache_work->DeviceContext) < 0)
        return;

#ifndef USE_DELAYED_CACHE_MODE
    spin_lock_irqsave(&xhci->cache_list_lock, flags);
    list_del_init(&ehub_cache_work->list);
    spin_unlock_irqrestore(&xhci->cache_list_lock, flags);
#endif /* ! USE_DELAYED_CACHE_MODE */

    dev_dbg(dev_ctx_to_dev(ehub_cache_work->DeviceContext), "ehub_cache_work 0x%p\n", ehub_cache_work);

    if (ehub_cache_work->ring_doorbell) {
        if (~(0) != ehub_cache_work->stream_id) {
            dev_dbg(dev_ctx_to_dev(ehub_cache_work->DeviceContext), "d_cw Ring Doorbell cw=0x%p SL=%d EP=%d\n",
                    ehub_cache_work,
                    ehub_cache_work->slot_id,
                    ehub_cache_work->ep_index);

            ehub_xhci_ring_ep_doorbell(xhci,
                                       ehub_cache_work->slot_id,
                                       ehub_cache_work->ep_index,
                                       ehub_cache_work->stream_id);
        } else {
            dev_dbg(dev_ctx_to_dev(ehub_cache_work->DeviceContext), "d_cw Ring Command Doorbell cw=0x%p\n",
                    ehub_cache_work);

            ehub_xhci_ring_cmd_db_low(xhci);
        }
    }

    ehub_cache_work->UrbContext->Status = URB_STATUS_COMPLETE;

    INIT_WORK(&ehub_cache_work->work, ehub_delayed_cache_write);

    spin_lock_irqsave(&xhci->cache_list_lock, flags);
    list_add_tail(&ehub_cache_work->list, &xhci->cache_queue_free);
    spin_unlock_irqrestore(&xhci->cache_list_lock, flags);
}

/**
* ehub_xhci_cache_work_expand - add new entries to the cache work queue.
* @xhci: xhci structure
* @entries: number of entries to add
*
* Create new cache work entries and add
* them to xhci->cache_wq_list_free.
*/
int
ehub_xhci_cache_work_expand(
    struct xhci_hcd *xhci,
    ulong entries,
    gfp_t flags
)
{
    int index = 0;

    dev_dbg(dev_ctx_to_dev(xhci->DeviceContext), "expand cache work entries: %lu\n", entries);
    for (index = 0; index < entries; index++) {
        struct cache_write_context *ehub_cache_work;

        ehub_cache_work = kzalloc(sizeof(struct cache_write_context),
                                  flags);
        if (NULL == ehub_cache_work)
            break;

        ehub_cache_work->DeviceContext = xhci->DeviceContext;

        INIT_LIST_HEAD(&ehub_cache_work->list);

        INIT_WORK(&ehub_cache_work->work, ehub_delayed_cache_write);

        ehub_cache_work->UrbContext = URB_Create(ehub_cache_work->DeviceContext,
                                                 ehub_cache_work->DeviceContext->UsbContext.UsbDevice,
                                                 ehub_cache_work->DeviceContext->UsbContext.UsbPipeCacheOut,
                                                 sizeof(EMBEDDED_CACHE_TRANSFER) + MESSAGE_DATA_BUFFER_SIZE_CACHE,
                                                 URB_CompletionRoutine_Cache,
                                                 ehub_cache_work,
                                                 flags);
        if (NULL == ehub_cache_work->UrbContext) {
            kfree(ehub_cache_work);
            break;
        }

        list_add_tail(&ehub_cache_work->list,
                      &xhci->cache_queue_free);
    }

    return index;
}


/* TODO: Use lib/genalloc.c for cache management. */
int
ehub_xhci_cache_create(
    struct xhci_hcd *xhci
    )
{
    u32* cacheControlRegisterOffset;
    int indexOfBlock;

    INIT_LIST_HEAD( &xhci->cache_list_free );
    INIT_LIST_HEAD( &xhci->cache_list_used );

    spin_lock_init( &xhci->cache_list_lock );

    INIT_LIST_HEAD( &xhci->cache_queue_free );
    INIT_LIST_HEAD( &xhci->cache_queue_used );

    xhci->ehub_cache_work.DeviceContext = xhci->DeviceContext;
    INIT_WORK(&xhci->ehub_cache_work.work, ehub_delayed_cache_write);

    xhci->number_of_caches_free = 0;
    xhci->number_of_caches_used = 0;

    xhci->ehub_cache_wq = alloc_workqueue("ehub_cache_wq", WQ_HIGHPRI | WQ_UNBOUND | WQ_MEM_RECLAIM, 0);
    if (!xhci->ehub_cache_wq)
        return -ENOMEM;

    cacheControlRegisterOffset = ( u32* )DEVHOST_CACHE_BAR_REG_ADDRESS;

    xhci_writel( xhci,
                 EHUB_CACHE_INITIALIZATION_REGISTER,
                 cacheControlRegisterOffset );

    if (0 > ehub_xhci_cache_work_expand(xhci, EHUB_CACHE_QUEUE_ENTRIES, GFP_KERNEL))
        return -ENOMEM;

    for (indexOfBlock = 0; indexOfBlock < EHUB_CACHE_NUMBER_OF_BLOCKS; indexOfBlock++) {
        PEHUB_CACHE_BLOCK cacheBlock;

        cacheBlock = kmalloc(sizeof(EHUB_CACHE_BLOCK),
                             GFP_KERNEL);

        cacheBlock->IndexOfBlock = indexOfBlock;
        cacheBlock->urb = NULL;
        cacheBlock->Address = EHUB_CACHE_START_ADDRESS + (EHUB_CACHE_BLOCK_SIZE * indexOfBlock);

        INIT_LIST_HEAD(&cacheBlock->list);

        list_add_tail(&cacheBlock->list,
                      &xhci->cache_list_free);

        xhci->number_of_caches_free++;
    }

    return 0;
}

void
ehub_xhci_cache_destroy(
    struct xhci_hcd *xhci
    )
{
    PEHUB_CACHE_BLOCK p, ehubCacheBlock;
    struct cache_write_context *q, *ehub_cache_work;

    FUNCTION_ENTRY;

    if (xhci->ehub_cache_wq)
        destroy_workqueue(xhci->ehub_cache_wq);

    list_for_each_entry_safe(ehubCacheBlock, p, &xhci->cache_list_free, list) {
        if (NULL != ehubCacheBlock) {
            list_del_init(&ehubCacheBlock->list);
            kfree(ehubCacheBlock);
        }
    }

    list_for_each_entry_safe(ehubCacheBlock, p, &xhci->cache_list_used, list) {
        if (NULL != ehubCacheBlock) {
            list_del_init(&ehubCacheBlock->list);
            kfree(ehubCacheBlock);
        }
    }

    list_for_each_entry_safe(ehub_cache_work, q, &xhci->cache_queue_free, list)
    {
        if (NULL != ehub_cache_work) {
            list_del_init(&ehub_cache_work->list);
            if (NULL != ehub_cache_work->UrbContext)
                URB_Destroy(ehub_cache_work->UrbContext);

            kfree(ehub_cache_work);
        }
    }

    list_for_each_entry_safe(ehub_cache_work, q, &xhci->cache_queue_used, list)
    {
        if (NULL != ehub_cache_work) {
            list_del_init(&ehub_cache_work->list);
            if (NULL != ehub_cache_work->UrbContext)
                URB_Destroy(ehub_cache_work->UrbContext);

            kfree(ehub_cache_work);
        }
    }

    FUNCTION_LEAVE;
}

PEHUB_CACHE_BLOCK
ehub_xhci_cache_block_allocate(
    struct xhci_hcd *xhci
    )
{
    PEHUB_CACHE_BLOCK cacheBlock;
    unsigned long flags;

    spin_lock_irqsave( &xhci->cache_list_lock, flags );

    if ( list_empty( &xhci->cache_list_free ) )
    {
        cacheBlock = NULL;
        spin_unlock_irqrestore( &xhci->cache_list_lock, flags );
        goto Exit;
    }

    cacheBlock = list_first_entry( &xhci->cache_list_free,
                                   EHUB_CACHE_BLOCK,
                                   list );

    cacheBlock->urb = NULL;

    list_move_tail( &cacheBlock->list,
                   &xhci->cache_list_used );
    xhci->number_of_caches_free--;
    xhci->number_of_caches_used++;

    dev_dbg(dev_ctx_to_dev(xhci->DeviceContext), "Alloc Block : %d used=%d free=%d\n",
            cacheBlock->IndexOfBlock, xhci->number_of_caches_used, xhci->number_of_caches_free);

    spin_unlock_irqrestore( &xhci->cache_list_lock, flags );

Exit:

    return cacheBlock;
}

PEHUB_CACHE_BLOCK
ehub_xhci_cache_block_allocate_segment(
    struct xhci_hcd *xhci,
    int cycle_state,
    void *start_addr
    )
{
    PEHUB_CACHE_BLOCK cacheBlock;
    unsigned long flags;
    int status;

    spin_lock_irqsave( &xhci->cache_list_lock, flags );

    if ( list_empty( &xhci->cache_list_free ) )
    {
        cacheBlock = NULL;
        spin_unlock_irqrestore( &xhci->cache_list_lock, flags );
        dev_warn(dev_ctx_to_dev(xhci->DeviceContext),
                 "WARNING cache_list_free is empty. used=%d free=%d\n",
                 xhci->number_of_caches_used, xhci->number_of_caches_free);
        goto Exit;
    }

    cacheBlock = list_first_entry( &xhci->cache_list_free,
                                   EHUB_CACHE_BLOCK,
                                   list );
    list_move_tail( &cacheBlock->list,
                   &xhci->cache_list_used );
    xhci->number_of_caches_free--;
    xhci->number_of_caches_used++;
    spin_unlock_irqrestore( &xhci->cache_list_lock, flags );

    status = ehub_queue_cache_write(xhci->DeviceContext,
                                    cacheBlock->Address,
                                    start_addr,
                                    EHUB_CACHE_BLOCK_SIZE,
                                    cycle_state,
                                    false,
                                    0,
                                    0,
                                    0);
    if (status < 0) {
        spin_lock_irqsave(&xhci->cache_list_lock, flags);
        dev_err(dev_ctx_to_dev(xhci->DeviceContext), "ERROR EMBEDDED_CACHE_Write fail %d\n", status);
        list_move_tail(&cacheBlock->list,
                       &xhci->cache_list_free);
        xhci->number_of_caches_used--;
        xhci->number_of_caches_free++;
        cacheBlock = NULL;
        spin_unlock_irqrestore(&xhci->cache_list_lock, flags);
        goto Exit;
    }

    dev_dbg(dev_ctx_to_dev(xhci->DeviceContext), "Alloc Block : %d used=%d free=%d\n",
            cacheBlock->IndexOfBlock, xhci->number_of_caches_used, xhci->number_of_caches_free );

Exit:

    return cacheBlock;
}

void
ehub_xhci_cache_block_free(
    struct xhci_hcd *xhci,
    PEHUB_CACHE_BLOCK EhubCacheBlock
    )
{
    unsigned long flags;

    FUNCTION_ENTRY;

    spin_lock_irqsave( &xhci->cache_list_lock, flags );

    list_move_tail( &EhubCacheBlock->list,
                   &xhci->cache_list_free );
    xhci->number_of_caches_used--;
    xhci->number_of_caches_free++;
    dev_dbg(dev_ctx_to_dev(xhci->DeviceContext), "Free Block : %d used=%d free=%d\n",
            EhubCacheBlock->IndexOfBlock, xhci->number_of_caches_used, xhci->number_of_caches_free );

    spin_unlock_irqrestore(&xhci->cache_list_lock, flags);

    FUNCTION_LEAVE;
}

void
ehub_xhci_cache_block_free_by_urb(
    struct xhci_hcd *xhci,
    struct urb* urb
    )
{
    unsigned long flags;
    struct list_head *item;
    PEHUB_CACHE_BLOCK ehubCacheBlock;
    int index;

    spin_lock_irqsave( &xhci->cache_list_lock, flags );

    xhci->TempEhubCacheBlockCount = 0;

    list_for_each( item, &xhci->cache_list_used )
    {
        ehubCacheBlock = list_entry( item,
                                     EHUB_CACHE_BLOCK,
                                     list );

        if ( ehubCacheBlock->urb == urb )
        {
            xhci->TempEhubCacheBlockArray[ xhci->TempEhubCacheBlockCount ] = ehubCacheBlock;
            xhci->TempEhubCacheBlockCount++;
        }
    }

    for ( index = 0; index < xhci->TempEhubCacheBlockCount; index++ )
    {
        xhci_dbg(xhci, "Free Block : %d \n", xhci->TempEhubCacheBlockArray[ index ]->IndexOfBlock );
        ehub_xhci_cache_block_free( xhci, xhci->TempEhubCacheBlockArray[ index ] );
    }

    spin_unlock_irqrestore( &xhci->cache_list_lock, flags );
}

union xhci_trb*
ehub_xhci_cache_get_trb(
    struct xhci_hcd *xhci,
    PEHUB_CACHE_BLOCK EhubCacheBlock,
    int IndexOfTrb
    )
{
    u64 addressTrb;
    union xhci_trb* tempTrb;

    addressTrb = EhubCacheBlock->Address + ( u64 )( IndexOfTrb* 16 );
    tempTrb = ( union xhci_trb* )addressTrb;

    return tempTrb;
}

static void ehub_find_prev_trb(
    union xhci_trb *trb, struct xhci_segment *seg,
    union xhci_trb **prev_trb, struct xhci_segment **prev_seg
)
{
    if (trb != seg->trbs) {
        *prev_trb = trb - 1;
        *prev_seg = seg;
        return;
    }

    for (*prev_seg = seg->next; *prev_seg != seg; *prev_seg = (*prev_seg)->next)
        if ((*prev_seg)->next == seg) {
            *prev_trb = ehub_xhci_get_link_trb_from_segment( (*prev_seg));
            return;
        }

    ASSERT(false);
    *prev_seg = NULL;
    *prev_trb = NULL;
}

void
ehub_xhci_cache_copy_from_ring(
    struct xhci_hcd *xhci,
    unsigned int slot_id,
    unsigned int ep_index,
    unsigned int stream_id,
    bool ring_doorbell
)
{
    u64 copyLength;
    union xhci_trb* start_trb;
    union xhci_trb* end_trb;
    union xhci_trb* enq_trb;
    union xhci_trb* deq_trb;
    union xhci_trb* cache_trb;
    union xhci_trb* last_trb;
    struct xhci_segment* cur_seg;
    struct xhci_segment* enq_seg;
    struct xhci_segment* deq_seg;
    struct xhci_segment* cache_seg;
    struct xhci_segment* last_seg;
    bool lastCopy;
    u64 trbOffset;
    u64 cacheAddress;
    int status;
    struct xhci_ring *ep_ring;

    ep_ring = xhci_triad_to_transfer_ring(xhci, slot_id,
                                          ep_index, stream_id);

    /* Capture locations in case it changes */
    /* Is this really needed? */
    deq_trb = ep_ring->dequeue;
    deq_seg = ep_ring->deq_seg;
    enq_trb = ep_ring->enqueue;
    enq_seg = ep_ring->enq_seg;
    cache_trb = ep_ring->cache_enqueue;
    cache_seg = ep_ring->cache_enq_seg;

    if ((enq_trb == deq_trb) &&
        (enq_seg == deq_seg)) {
        /* Ring shouldn't be empty */
        dev_err(dev_ctx_to_dev(xhci->DeviceContext),
                "ERROR Ring empty SL=%d EP=%d eq=0x%p dq=0x%p\n",
                slot_id, ep_index, enq_trb, deq_trb);
        ASSERT(false);
    }

    // First copy from the starting TRB to either the last Trb or
    // the end of the starting segment if the TD spans multiple segments.
    //
    start_trb = cache_trb;
    cur_seg = cache_seg;

    ehub_find_prev_trb(enq_trb, enq_seg, &last_trb, &last_seg);
    if (!last_trb || !last_seg) {
        dev_err(dev_ctx_to_dev(xhci->DeviceContext),
                "ERROR couldn't find previous trb\n");
        ASSERT(false);
    }

    do {
        /* Figure out how much we are going to copy. */
        if ((cur_seg != last_seg) ||
            (start_trb > last_trb)) {
            end_trb = ehub_xhci_get_link_trb_from_segment(cur_seg);
            lastCopy = false;
        } else {
            end_trb = last_trb;
            lastCopy = true;
        }

        trbOffset = ( u64 )start_trb - ( u64 )cur_seg->trbs;
        cacheAddress = cur_seg->EhubCacheBlock->Address;
        cacheAddress += trbOffset;

        copyLength = ( u32 )(1 + end_trb - start_trb) * sizeof(struct xhci_generic_trb);

        if (cacheAddress != 0 &&
            copyLength != 0) {
            status = ehub_queue_cache_write(xhci->DeviceContext,
                                            cacheAddress,
                                            ( u32* )start_trb,
                                            copyLength,
                                            0,
                                            lastCopy && ring_doorbell,
                                            slot_id,
                                            ep_index,
                                            stream_id);
            if (status < 0) {
                dev_err(dev_ctx_to_dev(xhci->DeviceContext), "ERROR EMBEDDED_CACHE_Write fail! %d\n", status);
                break;
            }
        }

        if (lastCopy) {
            break;
        }

        cur_seg = cur_seg->next;
        start_trb = cur_seg->trbs;

    } while (true);

    /* Check to see if these ever actually change. */
    ASSERT(enq_trb == ep_ring->enqueue);
    ASSERT(enq_seg == ep_ring->enq_seg);
    ASSERT(cache_trb == ep_ring->cache_enqueue);
    ASSERT(cache_seg == ep_ring->cache_enq_seg);

    ep_ring->cache_enqueue = enq_trb;
    ep_ring->cache_enq_seg = enq_seg;
}

union xhci_trb*
ehub_xhci_get_last_trb_from_segment(
    struct xhci_segment *seg
    )
{
    union xhci_trb* trb;
    trb = ( union xhci_trb* )(( u64 )seg->trbs + ( u64 )( LAST_TRB_INDEX * sizeof( struct xhci_generic_trb ) ) );
    return trb;
}

union xhci_trb*
ehub_xhci_get_link_trb_from_segment(
    struct xhci_segment *seg
    )
{
    union xhci_trb* trb;
    trb = ( union xhci_trb* )(( u64 )seg->trbs + ( u64 )( LINK_TRB_INDEX * sizeof( struct xhci_generic_trb ) ) );
    return trb;
}


#endif /* USE_TRB_CACHE_MODE */

u64
ehub_xhci_get_data_buffer_addr( struct xhci_hcd *xhci, struct urb *urb,
    u64 addr, int trb_buff_len )
{
    u64 addrData;
#ifdef USE_DATA_CACHE_MODE
    if (!usb_urb_dir_in(urb)) {
        PEHUB_CACHE_BLOCK ehubCacheBlock;

        ehubCacheBlock = ehub_xhci_cache_block_allocate(xhci);
        if (NULL != ehubCacheBlock) {
            int status;

            ehubCacheBlock->urb = urb;

            status = EMBEDDED_CACHE_Write(xhci->DeviceContext,
                                            ehubCacheBlock->Address,
                                            ( u32* )addr,
                                            trb_buff_len,
                                            0);
            if (status < 0) {
                xhci_err(xhci, "EMBEDDED_CACHE_Write fail %d\n", status);
                return 0;
            }

            addrData = 0;
            addrData = ehubCacheBlock->Address;
        } else {
            // panic( "\n\n#### ehub out of cache. ###\n\n" );
            //
            addrData = 0;
        }
    } else {
        // In transfer.
        //
        addrData = addr;
    }
#else
    addrData = addr;
#endif
    return addrData;
}

void ehub_xhci_handle_queued_port_status(struct work_struct* work_context)
{
    struct event_work_context *event_context;
    struct xhci_hcd *xhci;
    union xhci_trb *event;

    event_context = container_of( work_context,
                                  struct event_work_context,
                                  work.work );

    xhci = event_context->xhci;
    if (0 == DEVICECONTEXT_ErrorCheck(xhci->DeviceContext)) {
        event = (union xhci_trb *)(&event_context->event);
        handle_port_status(xhci, event);
    }
    kfree(work_context);
}

void ehub_xhci_queue_port_status_event(struct xhci_hcd *xhci,union xhci_trb *event)
{
    struct event_work_context *work_context = kzalloc(sizeof( *work_context ), GFP_KERNEL);

    ASSERT( work_context != NULL );

    work_context->xhci = xhci;
    work_context->event = event->generic;

    INIT_DELAYED_WORK(&work_context->work, ehub_xhci_handle_queued_port_status);
    schedule_delayed_work( &work_context->work, 10 );
}

void ehub_xhci_get_configuration( struct usb_device *udev, int* configuration_index )
{
    int ret;

    *configuration_index = 0;
    ret = usb_control_msg( udev,
                           usb_rcvctrlpipe( udev, 0 ),
                           USB_REQ_GET_CONFIGURATION,
                           USB_DIR_IN | USB_RECIP_DEVICE,
                           0,
                           0,
                           ( u8* )configuration_index,
                           1,
                           USB_CTRL_GET_TIMEOUT );
    ASSERT( ret >= 0 );
}

void ehub_xhci_set_configuration( struct usb_device *udev, int configuration_index )
{
    int ret;

    ret = usb_control_msg( udev,
                           usb_sndctrlpipe( udev, 0 ),
                           USB_REQ_SET_CONFIGURATION,
                           0,
                           configuration_index,
                           0,
                           NULL,
                           0,
                           USB_CTRL_SET_TIMEOUT );
    ASSERT( ret >= 0 );
}
