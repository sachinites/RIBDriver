#include <linux/module.h>
//#include <linux/init.h>

#include <linux/kernel.h>       /* printk() */
#include <linux/fs.h> 
#include <linux/cdev.h>

#include "dev_majors.h"
#include "common/kernutils.h"
#include "MAC/mac_table.h"

MODULE_AUTHOR("Abhishek Sagar");
MODULE_LICENSE("Dual BSD/GPL");

/* prototypes*/
static int char_driver_init_module(void);
static void char_driver_cleanup_module(void);

/* Global variables*/

static dev_t dev;
struct mac_table *mac = NULL;
/* Impomac device file operations*/

extern struct file_operations mac_fops;
//extern spinlock_t cross_bndry_spin_lock;
extern struct semaphore mac_serialize_readers_cs_sem;

int
char_driver_init_module(void){
	int rc = SUCCESS;

	printk(KERN_INFO "%s() is called\n", __FUNCTION__);
	sema_init(&mac_serialize_readers_cs_sem, 1);

	/* 1. mac_table device registration*/	
	{
		dev = MKDEV(MAC_MAJOR_NUMBER, 0);
		rc = register_chrdev_region(dev, MAC_MINOR_UNITS, "MAC_TABLE");
		if(rc < 0) goto MAJOR_NUMBER_REG_FAILED;

		mac =  init_mac_table();
		
		if(!mac) goto NO_MEM;

		/*Get the major and minor no which has been registered*/
		dev = MKDEV(MAC_MAJOR_NUMBER, 1);
		cdev_init(&mac->cdev, &mac_fops);
		mac->cdev.owner = THIS_MODULE;
		mac->cdev.ops = &mac_fops;
		/* add device to kernel finally*/
		rc = cdev_add (&mac->cdev, dev, MAC_MINOR_UNITS);
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
	unregister_chrdev_region(dev, MAC_MINOR_UNITS);  
	cdev_del(&mac->cdev);
	cleanup_mac_table(&mac);
	printk(KERN_INFO "Good by LKM!!\n"); 
}


module_init(char_driver_init_module);
module_exit(char_driver_cleanup_module);

