DRIVER_NAME = xilinx-axidma
$(DRIVER_NAME)-objs = axi_dma.o axidma_chrdev.o axidma_dma.o axidma_of.o
obj-m := $(DRIVER_NAME).o

SRC := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC)

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c
	rm -f Module.markers Module.symvers modules.order
	rm -rf .tmp_versions Modules.symvers
