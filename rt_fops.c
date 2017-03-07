#include "rt_fops.h"
#include "kern_usr.h"
#include "rt_table.h"
#include <asm/uaccess.h> // for datums

extern struct rt_table *rt;
struct file_operations rt_fops = {
	.owner 		=     THIS_MODULE,
	.llseek 	=     NULL, 
	.read 		=     rt_read,
	.write 		=     rt_write,
	.unlocked_ioctl =     NULL, 
	.open 		=     rt_open,
	.release 	=     rt_release,
	.unlocked_ioctl =     ioctl_rt_handler1,
	.compat_ioctl   =     ioctl_rt_handler2,
};

/* internal functions*/

#define SET_ACCESS_FLAG(filp, flag)	(filep->f_flags = filep->f_flags | flag)
#define UNSET_ACCESS_FLAG(filp, flag)   (filep->f_flags = filep->f_flags & (flag ^ 0xFFFFFFFF))

static 
void print_file_flags(struct file *filp){

	printk(KERN_INFO "filp->f_flags:\n");
	if(filp->f_flags & O_APPEND)
		printk(KERN_INFO "O_APPEND set\n");
	if(filp->f_flags & O_CREAT)
		printk(KERN_INFO "O_CREAT set\n");
	if(filp->f_flags & O_RDONLY)
		printk(KERN_INFO "O_RDONLY set\n");
	if(filp->f_flags & O_WRONLY)
		printk(KERN_INFO "O_WRONLY set\n");
	if(filp->f_flags & O_RDWR)
		printk(KERN_INFO "O_RDWR set\n");
	if(filp->f_flags & O_NONBLOCK)
		printk(KERN_INFO "O_NONBLOCK set\n");
}


int rt_open (struct inode *inode, struct file *filp){

	printk(KERN_INFO "%s() is called , inode = 0x%x, filep = 0x%x\n", 
		__FUNCTION__, (unsigned int)inode,  (unsigned int) filp);
	print_file_flags(filp);
        filp->private_data = rt;	
	return 0;
}

int rt_release (struct inode *inode, struct file *filp){
	
	struct rt_table *rt = NULL;
	rt = (struct rt_table *)filp->private_data;

	printk(KERN_INFO "%s() is called , inode = 0x%x, filep = 0x%x\n", __FUNCTION__, (unsigned int)inode,  (unsigned int) filp);
	return 0;
}


ssize_t rt_read (struct file *filp, char __user *buf, size_t count, loff_t *f_pos){
	
	ssize_t n_count = 0;
	
#if 0
	printk(KERN_INFO "%s() is called , filep = 0x%x, user buff = 0x%x, buff_size = %d\n", 
			__FUNCTION__, (unsigned int) filp, (unsigned int) buf, count);
#endif
	rt = (struct rt_table *)filp->private_data;

	down_read(&rt->rw_sem);
	n_count = copy_rt_table_to_user_space(rt, buf, count);
	up_read(&rt->rw_sem);

	return n_count;
}


ssize_t rt_write (struct file *filp, const char __user *buf, size_t count, loff_t *f_pos){

	rt = (struct rt_table *)(filp->private_data);
	printk(KERN_INFO "%s() is called , filep = 0x%x, user buff = 0x%x, buff_size = %d\n",
			__FUNCTION__, (unsigned int) filp, (unsigned int) buf, count);
	return 0;
}

long ioctl_rt_handler1 (struct file *filep, unsigned int cmd, unsigned long arg){
	long rc = 0;
	rt = (struct rt_table *)filep->private_data;

	printk(KERN_INFO "%s() is called , filep = 0x%x, cmd code = %u, rt = 0x%x, user_addr = 0x%x\n", 
				__FUNCTION__, (unsigned int) filep, cmd, (unsigned int)rt, (unsigned int)arg);

	switch(cmd){
		case (RT_IOC_RTPURGE):
			down_write(&rt->rw_sem);
			purge_rt_table(rt);
			up_write(&rt->rw_sem);
			break;
		case (RT_IOC_CR_RTENTRY):
			{
				struct rt_entry entry;
				copy_from_user((void *)&entry, (const void __user *)arg, sizeof(struct rt_entry));
				down_write(&rt->rw_sem);
				add_rt_table_entry_by_val(rt, entry);
				up_write(&rt->rw_sem);
			}
			break;
		case (RT_IOC_D_RTENTRY):
			break;
		case (RT_IOC_U_RTENTRY):
			break;
		case (RT_IOC_LOOKUP_RTENTRY):
			break;
		case (RT_IOC_GETRT):
			break;
		case (RT_IOC_RTDESTROY):
			break;
		case (RT_IOC_RTOPEN):
			break;
		case (RT_IOC_RTCLOSE):
			break;
		case (RT_IOC_SET_ACCESS_MODE):
			break;
		case (RT_IOC_GET_RT_INFO):
			{
				struct rt_info_t *rt_info = (struct rt_info_t *)arg;
				unsigned int actual_count = 0, counter = 0;
				struct singly_ll_node_t *head = GET_FIRST_RT_ENTRY_NODE(rt);

				if(access_ok(VERIFY_WRITE, (void __user*)arg, sizeof(struct rt_info_t)) ==  0){
					printk(KERN_INFO "%s() : invalid user space ptr\n", __FUNCTION__);
					return rc;
				}
				
				down_read(&rt->rw_sem);
				counter = GET_RT_ENTRY_COUNT(rt);
				copy_to_user((void __user *)&(rt_info->node_count), &counter, sizeof(unsigned int));
	
				while(head){
					actual_count++;
					head = GET_RT_NXT_ENTRY_NODE(head);
				}	

				// copying using datums	
				put_user(actual_count, &(rt_info->actual_node_count));

				up_read(&rt->rw_sem);
#if 0
				copy_to_user((void __user *)&(rt_info->actual_node_count), &actual_count, 
						sizeof(unsigned int));
#endif
				rc = 0;	
			}
			break;
		default:
			printk(KERN_INFO "No ioctl defined\n");
			return -ENOTTY;

	}
	return rc;
}


long ioctl_rt_handler2 (struct file *filep, unsigned int cmd, unsigned long arg){
	printk(KERN_INFO "%s() is called , filep = 0x%x, cmd code = %d\n", __FUNCTION__, (unsigned int) filep, cmd);
	return 0;
}

