obj-m	+=	motor_driver/motor_driver.o
obj-m	+=	echo_driver/echo_driver.o

KDIR	?=	$(error Set KDIR : make KDIR=/path/to/petalinux/build/tmp/work/.../linux-xlnx-build ARCH=arm CROSS_COMPILE=...)
PWD		:=	$(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
