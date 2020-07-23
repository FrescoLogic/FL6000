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

#ifndef EHUB_DEVICE_CONTEXT_H
#define EHUB_DEVICE_CONTEXT_H

#define MAX_PACKET_SIZE_BULK        ( 0x400 )
#define MAX_PACKET_SIZE_INTERRUPT   ( 0x400 )
#define MAX_PACKET_SIZE_ISOCH       ( 0x400 )

#define NUMBER_OF_MESSAGE_BULK      ( 8 )
#define NUMBER_OF_MESSAGE_INTERRUPT ( 8 )
#define NUMBER_OF_MESSAGE_ISOCH     ( 6 )

/* Large values here can delay Urb completions to the device driver.
 * 16 is a potential 2ms delay.  If the device driver is using Isoch
 * Urbs that have a small number of packets and isn't queuing up enough
 * transfers then this can be a problem. */
#define NUMBER_OF_MESSAGE_ISOCH_PKT ( 16 )
#define NUMBER_OF_MESSAGE_DOORBELL  ( 32 )

#define MESSAGE_DATA_BUFFER_SIZE_BULK       ( 8 * MAX_PACKET_SIZE_BULK )
#define MESSAGE_DATA_BUFFER_SIZE_INTERRUPT  ( 3 * MAX_PACKET_SIZE_INTERRUPT )
#define MESSAGE_DATA_BUFFER_SIZE_ISOCH      ( 6 * MAX_PACKET_SIZE_ISOCH )
/* This may need to change if cache is switch to Bulk endpoint. */
#define MESSAGE_DATA_BUFFER_SIZE_CACHE      (EHUB_CACHE_BLOCK_SIZE * (EHUB_INTERFACE_ALTERNATE_SETTING_INTERRUPT + 1))

#define DATA_LENGTH_EMBEDDED_REGISTER_READ_OUT_COMMAND    ( 4 )
#define DATA_LENGTH_EMBEDDED_REGISTER_WRITE               ( 8 )

#define DATA_INDEX_EMBEDDED_REGISTER_READ                 ( 3 )

#define WORK_ITEM_FROM_NOPLACE              ( 0 )
#define WORK_ITEM_FROM_REGISTER_READ        ( 1 )
#define WORK_ITEM_FROM_REGISTER_WRITE       ( 2 )
#define WORK_ITEM_FROM_CACHE_WRITE          ( 3 )
#define WORK_ITEM_FROM_EMBEDDED_MEMORY_READ ( 4 )

#define EHUB_MIN_VALID_VADDR              (0x00100000)

typedef struct _USB_CONTEXT_
{
	struct usb_device* UsbDevice;
	struct device* Device;
	void *xhci_hcd;
	unsigned int UsbPipeBulkIn;
	unsigned int UsbPipeBulkOut;
	unsigned int UsbPipeInterruptIn;
	unsigned int UsbPipeInterruptOut;
	unsigned int UsbPipeIsochIn;
	unsigned int UsbPipeIsochOut;
	unsigned int IsUsbHcdInvalid;
	unsigned int UsbPipeCacheOut;
	unsigned int UsbPipeDoorbellOut;
} USB_CONTEXT, *PUSB_CONTEXT;

typedef struct _WORK_ITEM_CONTEXT_
{
	struct list_head list;
	void* DeviceContextPvoid;
	struct delayed_work WorkItem;
	void ( *WorkItemProcess )( struct work_struct * );
	u8* DataBuffer;
	int DataBufferLength;
	int FromWhere;
} WORK_ITEM_CONTEXT, *PWORK_ITEM_CONTEXT;

typedef struct _URB_CONTEXT_
{
	void* DeviceContextPvoid;
	struct urb* Urb;
	struct completion Event;
	u8* DataBuffer;
	u32 DataBufferLength;
	dma_addr_t DataBufferDma;
	struct usb_device *Dev;
	int UrbCompletionStatus;
	int ActualLength;
	int Status;
	struct list_head list;
} URB_CONTEXT, *PURB_CONTEXT;

typedef struct _MICROFRAME_COUNTER_CONTEXT_
{
	u64 PerformanceFrequency;
	u64 TicksPerMicroframe;
	u64 LastPerformanceCount;
	u32 LastMicroframeCount;
} MICROFRAME_COUNTER_CONTEXT, *PMICROFRAME_COUNTER_CONTEXT;

typedef struct _CONTROL_FLAGS_
{
	u32 IsEhubXhciInitReady;
} CONTROL_FLAGS, *PCONTROL_FLAGS;

typedef struct _ERROR_FLAGS_
{
	u32 UrbContextAllocationError;
	u32 UsbAllocateUrbError;
	u32 UsbAllocateCoherentError;
	u32 EmbeddedRegisterError;
	u32 EmbeddedCacheWriteError;
} ERROR_FLAGS, *PERROR_FLAGS;

typedef struct _DEVICE_CONTEXT_
{
	struct usb_interface* InterfaceBackup;

	USB_CONTEXT UsbContext;

	/* TODO: EHUB rename generic *Locks as either Mutex or Semaphore they stop changing. */
	struct mutex MessageHandleEmbeddedMemoryReadCompletionLock;
	struct semaphore EmbeddedRegisterLock;

	spinlock_t SpinLockEmbeddedDoorbellWrite;

	struct list_head doorbell_list_free;
	struct list_head doorbell_list_busy;

	struct list_head WorkItemPendingQueue;
	struct list_head WorkItemProcessingQueue;

	struct workqueue_struct *WorkItemQueue;
	spinlock_t SpinLockWorkItemQueue;
	int NumberOfWorkItemInProcessingQueue;

	int EmbeddedRegisterReadOccupy;
	int EmbeddedRegisterWriteOccupy;
	int EmbeddedCacheWriteOccupy;

	struct completion CompletionEventEmbeddedRegisterRead;
	u32 DataEmbeddedRegisterRead;

	int UrbPendingCount;

	PURB_CONTEXT UrbContextEmbeddedRegisterRead;
	PURB_CONTEXT UrbContextEmbeddedRegisterWrite;

	/* TODO: Use a safe method to select entry to use instead of just
	 * grabbing next entry. */
	PURB_CONTEXT UrbContextEmbeddedMemoryReadCompletion[ NUMBER_OF_MESSAGE_BULK ];
	int CurrentEmbeddedMemoryReadCompletionUrbContextIndex;

	PURB_CONTEXT UrbContextMessageBulk[ NUMBER_OF_MESSAGE_BULK ];
	PURB_CONTEXT UrbContextMessageInterrupt[ NUMBER_OF_MESSAGE_BULK ];
	PURB_CONTEXT UrbContextMessageIsoch[ NUMBER_OF_MESSAGE_ISOCH ];

	ERROR_FLAGS     ErrorFlags;
	CONTROL_FLAGS   ControlFlags;

	unsigned int last_uframe_cnt;
	ktime_t last_uframe_time;
	ktime_t pending_uframe_time;

	struct delayed_work stop_isoch_work;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

#define IS_URB_ERROR( _DeviceContext_ )                      ( _DeviceContext_->ErrorFlags.UrbContextAllocationError )
#define IS_EMBEDDED_CACHE_WRITE_ERROR( _DeviceContext_ )     ( _DeviceContext_->ErrorFlags.EmbeddedCacheWriteError )

PDEVICE_CONTEXT
DEVICECONTEXT_Create(struct device *dev);

void
DEVICECONTEXT_Destroy(
	PDEVICE_CONTEXT DeviceContext
	);

int
DEVICECONTEXT_ErrorCheck(
	PDEVICE_CONTEXT DeviceContext
	);

#endif
