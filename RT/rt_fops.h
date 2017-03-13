#ifndef __RT_FOPS_H__
#define __RT_FOPS_H__

#include <linux/fs.h>
/* Routing table driver operations */
int rt_open     (struct inode *inode, struct file *filp);
int rt_release  (struct inode *inode, struct file *filp);
ssize_t rt_read (struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
ssize_t rt_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
long ioctl_rt_handler1 (struct file *filep, unsigned int cmd, unsigned long arg);
long ioctl_rt_handler2 (struct file *filep, unsigned int cmd, unsigned long arg);
unsigned int rt_poll (struct file *filep, struct poll_table_struct *poll_table);
#endif
