# Makefile
#
# Fresco Logic FL6000 F-One Controller Driver
#
# Copyright (C) 2014-2017 Fresco Logic, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#

# tell define_trace.h where to find the xhci trace header
CFLAGS_ehub-xhci-trace.o := -I$(src)

ehub-y := ehub_module.o
ehub-y += ehub_usb.o
ehub-y += ehub_urb.o
ehub-y += ehub_device_context.o
ehub-y += ehub_work_item.o
ehub-y += ehub_register.o
ehub-y += ehub_embedded_register.o
ehub-y += ehub_embedded_cache.o
ehub-y += ehub_notification.o
ehub-y += ehub_message.o
ehub-y += ehub-xhci-trace.o
ehub-y += xhci.o xhci-mem.o
ehub-y += xhci-ring.o xhci-hub.o xhci-dbg.o
ehub-y += xhci-ehub.o

ifdef CONFIG_USB_EHUB

obj-$(CONFIG_USB_EHUB) := ehub.o

else

obj-m := ehub.o

#KERNEL_PATH = ../../../kernel/linux-3.10.0-327.13.1.el7
#KERNEL_PATH = /usr/src/kernels/3.10.0-327.13.1.el7.x86_64
KERNEL_PATH = /usr/src/kernels/`uname -r`

ifdef CONFIG_USB_EHUB_DEBUG_MODE
EXTRA_CFLAGS += -g -ggdb3 -O0 -DEHUB_DEBUG_ENABLE
endif

all:
	make -C $(KERNEL_PATH) M=$(PWD) modules

clean:
	make -C $(KERNEL_PATH) M=$(PWD) clean
	rm -f Module.symvers

endif

