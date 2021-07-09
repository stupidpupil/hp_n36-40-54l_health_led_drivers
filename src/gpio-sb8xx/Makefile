obj-m += gpio-sb8xx.o
KVER ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVER)/build
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
