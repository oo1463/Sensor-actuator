export CROSS_COMPILE=

obj-m := sa_driver.o

KERNELDIR:=~/linux-rpi/
ARM:= ARCH=arm CROSS_COMPILE=/usr/bin/arm-linux-gnueabi-
PWD:= $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) $(ARM) modules
	arm-linux-gnueabi-gcc sa_app.c -o sa_app -lpthread
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) $(ARM) clean
	rm sa_app

scp:
	scp $(obj-m:.o=.ko) sa_app mknod.sh pi@183.99.21.144:~/