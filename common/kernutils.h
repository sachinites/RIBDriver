#ifndef __COMMON__
#define __COMMON__

#define assert(x)                                                       \
do {    if (x) break;                                                   \
        printk(KERN_EMERG "### ASSERTION FAILED %s: %s: %d: %s\n",      \
               __FILE__, __func__, __LINE__, #x); dump_stack(); BUG();  \
} while (0);

#define SUCCESS 0
#define FAILURE -1

#define SET_FILE_ACCESS_FLAG(filp, flag)        (filep->f_flags = filep->f_flags | flag)
#define UNSET_FILE_ACCESS_FLAG(filp, flag)      (filep->f_flags = filep->f_flags & (flag ^ 0xFFFFFFFF))

#define IS_FILE_ACCESS_FLAG_SET(filp, flag)	(filep->f_flags & flag)
#define IS_FILE_ACCESS_FLAG_NOT SET(filp, flag)	(!(filep->f_flags & flag))

struct file;
void print_file_flags(struct file *filp);


#define  SEM_LOCK(sem)	{if(down_interruptible(sem)) return -ERESTARTSYS;} 
#define  SEM_UNLOCK(sem)(up(sem))

#endif
