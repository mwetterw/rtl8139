obj-m += r8139d_naive.o
r8139d_naive-objs := main.o pci.o net.o hw.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
