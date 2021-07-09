obj-m += i2c-piix4.o
KVER ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVER)/build
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
