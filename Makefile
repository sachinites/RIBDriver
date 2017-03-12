obj-m += mac_table_drv.o rt_table_drv.o

rt_table_drv-objs += rt_register_drv.o common/LinkedListApi.o RT/rt_table.o RT/rt_fops.o common/kernthread.o common/Queue.o common/kernutils.o

mac_table_drv-objs += mac_register_drv.o common/LinkedListApi.o MAC/mac_table.o MAC/mac_fops.o common/kernthread.o common/Queue.o common/kernutils.o
all:
	make -C /lib/modules/4.1.27/build ARCH=um M=$(PWD) modules
clean:
	make -C /lib/modules/4.1.27/build ARCH=um M=$(PWD) clean
