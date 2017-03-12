#ifndef __MAC_FOPS_H__
#define __MAC_FOPS_H__

#include <linux/fs.h>
/* mac table driver operations */
int mac_open     (struct inode *inode, struct file *filp);
int mac_release  (struct inode *inode, struct file *filp);
ssize_t mac_read (struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
ssize_t mac_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
long ioctl_mac_handler1 (struct file *filep, unsigned int cmd, unsigned long arg);
long ioctl_mac_handler2 (struct file *filep, unsigned int cmd, unsigned long arg);

#endif
