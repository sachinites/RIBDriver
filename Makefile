obj-m += rt_table_drv.o
rt_table_drv-objs += register_drv.o LinkedListApi.o rt_table.o rt_fops.o kernthread.o Queue.o
all:
	make -C /lib/modules/`uname -r`/build M=$(PWD) modules
clean:
	make -C /lib/modules/`uname -r`/build M=$(PWD) clean
