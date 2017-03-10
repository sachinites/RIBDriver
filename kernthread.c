#include "kernthread.h"
#include <linux/slab.h>
#include <linux/spinlock.h>


struct kernthread *new_kern_thread(arg_fn fn, void *fn_arg, char *th_name){
	struct kernthread *kernthread = kzalloc(sizeof(struct kernthread),GFP_KERNEL);
	init_completion(&kernthread->completion);
	sema_init(&kernthread->sem, 1);
	init_rwsem(&kernthread->rw_sem);
	spin_lock_init(&kernthread->spin_lock);
	kernthread->busy_mode = IDLE;
	kernthread->fn_arg = fn_arg;
	kernthread->task = kthread_create(fn, (void *)kernthread, th_name);
	/*The thread will not start running immediately; to get the thread to run, 
	pass the task_struct pointer returned by kthread_create() to wake_up_process().*/
	wake_up_process(kernthread->task);
	return kernthread;
}


void cleanup_kernthread(struct kernthread *kernthread){
}

enum busy_mode_t
get_kern_thread_busy_mode( struct kernthread *kernthread){
	return kernthread->busy_mode;
}

char *get_str_busy_mode(enum busy_mode_t mode){
	switch(mode){
		case BUSY_WITH_WRITERS:
			return "BUSY_WITH_WRITERS";
		case BUSY_WITH_READERS:	
			return "BUSY_WITH_READERS";
		case IDLE:
			return "IDLE";
		default:
			return "UNKNOWN";
	}
}
