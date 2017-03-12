#ifndef __KERNUTILS__
#define __KERNUTILS__

#include <linux/fs.h>

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

#endif
