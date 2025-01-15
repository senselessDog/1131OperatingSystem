obj-m += kfetch_mod_313551137.o

KDIR := /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

load:
	sudo insmod kfetch_mod_313551137.ko

unload:
	sudo rmmod kfetch_mod_313551137

.PHONY: all clean load unload
