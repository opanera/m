ifeq ($(KERNELRELEASE),)

PWD := $(shell pwd)
KERNELDIR ?= /home/kevin/Workspace/Demo/kernel/linux-2.6.35-farsight
INSTALLDIR ?= /home/kevin/Workspace/Demo/rootfs

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

modules_install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) INSTALL_MOD_PATH=$(INSTALLDIR) modules_install
clean:
	rm -rf *.o *.ko *.mod.c .*.cmd modules.order Module.symvers .tmp_versions

else

obj-m := fspwm.o

endif
