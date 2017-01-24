obj-m += r8139d_naive.o
r8139d_naive-objs := main.o pci.o net.o hw.o

myflags = -D__CHECK_ENDIAN__

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) C=2 CF="$(myflags)" modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
