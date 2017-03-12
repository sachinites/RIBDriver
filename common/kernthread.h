#ifndef __KERNTHREAD_API__
#define __KERNTHREAD_API__

#include<linux/kthread.h>
#include <linux/semaphore.h>

typedef int (*arg_fn)(void *);

enum busy_mode_t{
	BUSY_WITH_WRITERS,
	BUSY_WITH_READERS,
	IDLE
};

struct kernthread{
	struct task_struct *task;
	struct completion completion;// equivalent to pthread_cond_variable
	struct semaphore sem;
	struct rw_semaphore rw_sem;
	spinlock_t spin_lock;
	enum busy_mode_t busy_mode; // applicable only for worker thread
	void *fn_arg;
};

struct kernthread *
new_kern_thread(arg_fn fn, void *fn_arg, char *th_name);

void cleanup_kernthread(struct kernthread *kernthread);

enum busy_mode_t
get_kern_thread_busy_mode( struct kernthread *kernthread);

char *
get_str_busy_mode(enum busy_mode_t mode);

#endif 


