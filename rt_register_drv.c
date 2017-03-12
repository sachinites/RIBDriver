#include <linux/module.h>
//#include <linux/init.h>

#include <linux/kernel.h>       /* printk() */
#include <linux/fs.h> 
#include <linux/cdev.h>

#include "dev_majors.h"
#include "common/kernutils.h"
#include "RT/rt_table.h"

MODULE_AUTHOR("Abhishek Sagar");
MODULE_LICENSE("Dual BSD/GPL");

/* prototypes*/
static int char_driver_init_module(void);
static void char_driver_cleanup_module(void);

/* Global variables*/

static dev_t dev;
struct rt_table *rt = NULL;
/* Import device file operations*/

extern struct file_operations rt_fops;
//extern spinlock_t cross_bndry_spin_lock;
extern struct semaphore rt_serialize_readers_cs_sem;

int
char_driver_init_module(void){
	int rc = SUCCESS;

	printk(KERN_INFO "%s() is called\n", __FUNCTION__);
	sema_init(&rt_serialize_readers_cs_sem, 1);

	/* 1. rt_table device registration*/	
	{
		dev = MKDEV(RT_MAJOR_NUMBER, 0);
		rc = register_chrdev_region(dev, RT_MINOR_UNITS, "RT_TABLE");
		if(rc < 0) goto MAJOR_NUMBER_REG_FAILED;

		rt =  init_rt_table();
		
		if(!rt) goto NO_MEM;

		/*Get the major and minor no which has been registered*/
		dev = MKDEV(RT_MAJOR_NUMBER, 1);
		cdev_init(&rt->cdev, &rt_fops);
		rt->cdev.owner = THIS_MODULE;
		rt->cdev.ops = &rt_fops;
		/* add device to kernel finally*/
		rc = cdev_add (&rt->cdev, dev, RT_MINOR_UNITS);
		if(rc !=0) goto CDEV_ADD_FAILED;
	}

	return rc;

	MAJOR_NUMBER_REG_FAILED:
		printk(KERN_INFO "%s(): can't get major number %d from kernel\n", __FUNCTION__, MAJOR(dev));
		return rc;

	NO_MEM:
		printk(KERN_INFO "%s(): Out of memory error\n", __FUNCTION__);
		char_driver_cleanup_module();
		return rc;

	CDEV_ADD_FAILED:
		printk(KERN_INFO "%s(): Error %d adding device\n", __FUNCTION__, rc);
		char_driver_cleanup_module();
		return rc;
}

void
char_driver_cleanup_module(void){
	printk(KERN_INFO "%s() is called\n", __FUNCTION__);
	unregister_chrdev_region(dev, RT_MINOR_UNITS);  
	cdev_del(&rt->cdev);
	cleanup_rt_table(&rt);
	printk(KERN_INFO "Good by LKM!!\n"); 
}


module_init(char_driver_init_module);
module_exit(char_driver_cleanup_module);

