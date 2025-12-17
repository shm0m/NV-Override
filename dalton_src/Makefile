obj-m += dalton_drv.o
# UTILISER LE NOYAU COURANT (Vital pour que insmod marche)
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean