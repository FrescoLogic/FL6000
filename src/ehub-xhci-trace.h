/*
 * Fresco Logic FL6000 F-One Controller Driver
 *
 * Copyright (C) 2013 Xenia Ragiadakou
 * Modifications Copyright (C) 2014-2017 Fresco Logic, Inc.
 *
 * Author: Xenia Ragiadakou
 * Email : burzalodowa@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "ehub_defines.h"
#include "ehub_public.h"
#include "ehub_device_context.h"
#include "ehub_embedded_cache.h"
#include "xhci.h"

#ifdef EHUB_TRACING
#undef TRACE_SYSTEM
#define TRACE_SYSTEM ehub

/*
 * The TRACE_SYSTEM_VAR defaults to TRACE_SYSTEM, but must be a
 * legitimate C variable. It is not exported to user space.
 */
#undef TRACE_SYSTEM_VAR
#define TRACE_SYSTEM_VAR ehub

#if !defined(__EHUB_XHCI_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define __EHUB_XHCI_TRACE_H

#include <linux/tracepoint.h>

#define XHCI_MSG_MAX	500

DECLARE_EVENT_CLASS(ehub_xhci_log_msg,
	TP_PROTO(struct va_format *vaf),
	TP_ARGS(vaf),
	TP_STRUCT__entry(__dynamic_array(char, msg, XHCI_MSG_MAX)),
	TP_fast_assign(
		vsnprintf(__get_str(msg), XHCI_MSG_MAX, vaf->fmt, *vaf->va);
	),
	TP_printk("%s", __get_str(msg))
);

DEFINE_EVENT(ehub_xhci_log_msg, ehub_xhci_dbg_address,
	TP_PROTO(struct va_format *vaf),
	TP_ARGS(vaf)
);

DEFINE_EVENT(ehub_xhci_log_msg, ehub_xhci_dbg_context_change,
	TP_PROTO(struct va_format *vaf),
	TP_ARGS(vaf)
);

DEFINE_EVENT(ehub_xhci_log_msg, ehub_xhci_dbg_quirks,
	TP_PROTO(struct va_format *vaf),
	TP_ARGS(vaf)
);

DEFINE_EVENT(ehub_xhci_log_msg, ehub_xhci_dbg_reset_ep,
	TP_PROTO(struct va_format *vaf),
	TP_ARGS(vaf)
);

DEFINE_EVENT(ehub_xhci_log_msg, ehub_xhci_dbg_cancel_urb,
	TP_PROTO(struct va_format *vaf),
	TP_ARGS(vaf)
);

DEFINE_EVENT(ehub_xhci_log_msg, ehub_xhci_dbg_init,
	TP_PROTO(struct va_format *vaf),
	TP_ARGS(vaf)
);

DEFINE_EVENT(ehub_xhci_log_msg, ehub_xhci_dbg_ring_expansion,
	TP_PROTO(struct va_format *vaf),
	TP_ARGS(vaf)
);

DECLARE_EVENT_CLASS(ehub_xhci_log_ctx,
	TP_PROTO(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx,
		 unsigned int ep_num),
	TP_ARGS(xhci, ctx, ep_num),
	TP_STRUCT__entry(
		__field(int, ctx_64)
		__field(unsigned, ctx_type)
		__field(dma_addr_t, ctx_dma)
		__field(u8 *, ctx_va)
		__field(unsigned, ctx_ep_num)
		__field(int, slot_id)
		__dynamic_array(u32, ctx_data,
			((HCC_64BYTE_CONTEXT(xhci->hcc_params) + 1) * 8) *
			((ctx->type == XHCI_CTX_TYPE_INPUT) + ep_num + 1))
	),
	TP_fast_assign(
		struct usb_device *udev;

		udev = to_usb_device(xhci_to_hcd(xhci)->self.controller);
		__entry->ctx_64 = HCC_64BYTE_CONTEXT(xhci->hcc_params);
		__entry->ctx_type = ctx->type;
		__entry->ctx_dma = ctx->dma;
		__entry->ctx_va = ctx->bytes;
		__entry->slot_id = udev->slot_id;
		__entry->ctx_ep_num = ep_num;
		memcpy(__get_dynamic_array(ctx_data), ctx->bytes,
			((HCC_64BYTE_CONTEXT(xhci->hcc_params) + 1) * 32) *
			((ctx->type == XHCI_CTX_TYPE_INPUT) + ep_num + 1));
	),
	TP_printk("\nctx_64=%d, ctx_type=%u, ctx_dma=@%llx, ctx_va=@%p",
			__entry->ctx_64, __entry->ctx_type,
			(unsigned long long) __entry->ctx_dma, __entry->ctx_va
	)
);

DEFINE_EVENT(ehub_xhci_log_ctx, ehub_xhci_address_ctx,
	TP_PROTO(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx,
		 unsigned int ep_num),
	TP_ARGS(xhci, ctx, ep_num)
);

DECLARE_EVENT_CLASS(ehub_xhci_log_event,
	TP_PROTO(void *trb_va, struct xhci_generic_trb *ev),
	TP_ARGS(trb_va, ev),
	TP_STRUCT__entry(
		__field(void *, va)
		__field(u64, dma)
		__field(u32, status)
		__field(u32, flags)
		__dynamic_array(u8, trb, sizeof(struct xhci_generic_trb))
	),
	TP_fast_assign(
		__entry->va = trb_va;
		__entry->dma = ((u64)le32_to_cpu(ev->field[1])) << 32 |
					le32_to_cpu(ev->field[0]);
		__entry->status = le32_to_cpu(ev->field[2]);
		__entry->flags = le32_to_cpu(ev->field[3]);
		memcpy(__get_dynamic_array(trb), trb_va,
			sizeof(struct xhci_generic_trb));
	),
	TP_printk("\ntrb_dma=@%llx, trb_va=@%p, status=%08x, flags=%08x",
			(unsigned long long) __entry->dma, __entry->va,
			__entry->status, __entry->flags
	)
);

DEFINE_EVENT(ehub_xhci_log_event, ehub_xhci_cmd_completion,
	TP_PROTO(void *trb_va, struct xhci_generic_trb *ev),
	TP_ARGS(trb_va, ev)
);

#endif /* __EHUB_XHCI_TRACE_H */

/* this part must be outside header guard */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE ehub-xhci-trace

#include <trace/define_trace.h>
#else /* !EHUB_TRACING */
#include <linux/printk.h>
static inline void trace_ehub_xhci_dbg_init(struct va_format * unused)
{
	do {
		;
	} while (0);
}
#define trace_ehub_xhci_dbg_quirks trace_ehub_xhci_dbg_init
#define trace_ehub_xhci_dbg_context_change trace_ehub_xhci_dbg_init
#define trace_ehub_xhci_dbg_reset_ep trace_ehub_xhci_dbg_init
#define trace_ehub_xhci_dbg_cancel_urb trace_ehub_xhci_dbg_init
#define trace_ehub_xhci_dbg_address trace_ehub_xhci_dbg_init
#define trace_ehub_xhci_dbg_ring_expansion trace_ehub_xhci_dbg_init
static inline void trace_ehub_xhci_cmd_completion(void *trb_va, struct xhci_generic_trb *ev)
{
	do {
		;
	} while (0);
}
static inline void trace_ehub_xhci_address_ctx(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx,
								 unsigned int ep_num)
{
	do {
		;
	} while (0);
}
#endif /* !EHUB_TRACING */
