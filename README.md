# Fresco Logic FL6000 4-Port USB 3.0 F-One Controller Linux/Android driver

### 1. What is this?

This is an official driver release from Fresco Logic in an attempt to help the open-source community adopting the development and use of the FL6000 device.

### 2. On which kernel versions does this driver work?

This driver is tested on Ubuntu 14 LTS as well as some Android platforms with kernel version 3.10.x. and 3.18.x.
You might need to adapt it for your own use.

### 3. Target audience

This release is targeted to open-source developers, as opposed to end-users.

### 4. Limitations?

Support for isochronous devices is currently disabled.  Limited testing has been done with USB webcams with some success.  Isoch out support needs some work on data caching before it will function properly.

### 5. How do I compile & test the kernel driver?

Find your kernel source tree, and edit `src/Makefile`. Locate the following line:
    
    KERNEL_PATH = /usr/src/linux-headers-4.4.0-72-generic`
    
Modify this line so that it points to the correct source tree.
After that, run `make CONFIG_USB_EHUB_HCD=m` to create `ehub.ko` and run `insmod ehub.ko` to load the driver.

### 6. How do I file a bug to the Fresco Logic developers?

You can file bugs to [Github Issues](https://github.com/FrescoLogic/FL6000/issues)
