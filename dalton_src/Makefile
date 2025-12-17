obj-m += dalton_drv.o
KDIR := /usr/src/linux-headers-6.8.0-90-generic

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
