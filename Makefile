obj-m += rt_table_drv.o
rt_table_drv-objs += register_drv.o LinkedListApi.o rt_table.o rt_fops.o
all:
	make -C /lib/modules/4.1.27/build ARCH=um M=$(PWD) modules
clean:
	make -C /lib/modules/4.1.27/build ARCH=um M=$(PWD) clean