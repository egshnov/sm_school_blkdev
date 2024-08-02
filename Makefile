obj-m += main.o memtable.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

ci:
	make -C /lib/modules/linux-headers-6.2.0-1016-azure/build M=$(PWD) modules