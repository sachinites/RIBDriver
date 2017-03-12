#include "rt_fops.h"
#include "kernusr.h"
#include "rt_table.h"
#include <asm/uaccess.h> // for datums
#include <linux/slab.h>
#include "../common/kernutils.h"

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

/* Global variables*/
static unsigned int n_readers_to_be_service = 0; 
struct semaphore serialize_readers_cs_sem;
static struct rt_update_t *rt_update_vector = NULL;
static int rt_update_vector_count = 0;

int rt_open (struct inode *inode, struct file *filp){

	struct kernthread *kernthread = NULL;
	kernthread = kzalloc(sizeof(struct kernthread), GFP_KERNEL);
	printk(KERN_INFO "%s() is called , inode = 0x%x, filep = 0x%x\n", 
		__FUNCTION__, (unsigned int)inode,  (unsigned int) filp);
	print_file_flags(filp);

        init_completion(&kernthread->completion);
        sema_init(&kernthread->sem, 1);
        init_rwsem(&kernthread->rw_sem);
        spin_lock_init(&kernthread->spin_lock);
        kernthread->busy_mode = IDLE;
	kernthread->task = get_current();
	kernthread->fn_arg = NULL;
        filp->private_data = kernthread;
	return 0;
}

int rt_release (struct inode *inode, struct file *filp){
	
	printk(KERN_INFO "%s() is called , inode = 0x%x, filep = 0x%x\n", __FUNCTION__, (unsigned int)inode,  (unsigned int) filp);
	
	return 0;
}


ssize_t rt_read (struct file *filp, char __user *buf, size_t count, loff_t *f_pos){
	
	ssize_t n_count = 0;
	
	down_read(&rt->rw_sem);
	n_count = copy_rt_table_to_user_space(rt, buf, count);
	up_read(&rt->rw_sem);

	return n_count;
}


ssize_t rt_write (struct file *filp, const char __user *buf, size_t count, loff_t *f_pos){

	printk(KERN_INFO "%s() is called , filep = 0x%x, user buff = 0x%x, buff_size = %d\n",
			__FUNCTION__, (unsigned int) filp, (unsigned int) buf, count);
	return 0;
}

static int
allow_rt_access_to_readers(struct rt_table *rt, unsigned int n){
/* fn to allow n readers from rt reader q to allow rt access */
   unsigned int i = 0;
   struct kernthread *reader_thread = NULL;

   printk(KERN_INFO "inside %s() ....\n", __FUNCTION__);
	
   for (; i < n; i++){
	SEM_LOCK_READER_Q(rt);
	reader_thread = deque(rt->reader_Q);
	SEM_UNLOCK_READER_Q(rt);
	printk(KERN_INFO "giving rt access to reader thread : \"%s\" (pid %i)\n", 
			 reader_thread->task->comm, reader_thread->task->pid);	
	complete(&reader_thread->completion);
   }
   return 0;
}



int rt_worker_fn(void *arg){
	struct kernthread *worker_thread = NULL,
			  *writer_thread = NULL;

	printk(KERN_INFO "%s() is called is and activated. Waiting for the writer's arrival\n", __FUNCTION__);

	worker_thread = (struct kernthread * )arg;
	
	WAIT_FOR_WRITER_ARRIVAL:
	printk(KERN_INFO "worker thread waiting for writer's arrival Or scheduling the next writer\n");
	wait_event_interruptible(rt->writerQ, !(is_queue_empty(rt->writer_Q)));
	printk(KERN_INFO "worker thread is revived from sleep, picking up the first writer thread from writerQ\n");
	
	printk(KERN_INFO "worker thread is locking the writer Queue= 0x%x\n", (unsigned int)rt->writer_Q);
	SEM_LOCK_WRITER_Q(rt);
	
	writer_thread = (struct kernthread *)deque(rt->writer_Q);
	printk(KERN_INFO "worker thread has extracted the writer \"%s\" (pid %i) from writer Queue = 0x%x\n", 
			writer_thread->task->comm, writer_thread->task->pid, (unsigned int)rt->writer_Q);

	SEM_UNLOCK_WRITER_Q(rt);
	printk(KERN_INFO "worker thread has released locked over writer Queue= 0x%x\n", (unsigned int)rt->writer_Q);
	
	if(!writer_thread){
		printk(KERN_INFO "worker thread finds there is no writer in writer_Q, worker thread is sleeping\n");
		SEM_UNLOCK_WRITER_Q(rt);
		printk(KERN_INFO "worker thread has unlocked the writer Queue= 0x%x\n", (unsigned int)rt->writer_Q);
		goto WAIT_FOR_WRITER_ARRIVAL;
	}

	printk(KERN_INFO "worker thread signalling (completion) writer \"%s\" (pid %i) to access rt\n", 
			writer_thread->task->comm, writer_thread->task->pid);
	complete(&writer_thread->completion);
	printk(KERN_INFO "worker thread waiting for writer thread to finish with rt, calling wait_for_completion\n");
	worker_thread->busy_mode = BUSY_WITH_WRITERS;
	printk(KERN_INFO "worker thread busy mode status = %s\n", get_str_busy_mode(get_kern_thread_busy_mode(worker_thread)));
	wait_for_completion(&worker_thread->completion);
	printk(KERN_INFO "worker thread has recieved the wakeup signal from writer thread, woken up \n");
	printk(KERN_INFO "worker thread is calculating the no of readers in reader_q now\n");
	SEM_LOCK_READER_Q(rt);
	n_readers_to_be_service = Q_COUNT(rt->reader_Q);
	SEM_UNLOCK_READER_Q(rt);	
	printk(KERN_INFO "worker thread finds the no. of readers to be serviced  = %u\n", n_readers_to_be_service);

	if(n_readers_to_be_service == 0){
		printk(KERN_INFO "worker thread finds there are no readers\n");
		worker_thread->busy_mode = IDLE;
		/* delete all updates in rt change list as their are no readers*/
		rt_empty_change_list(rt);	
		goto WAIT_FOR_WRITER_ARRIVAL;
	}

	printk(KERN_INFO "worker thread is preparing update msg to service %u readers from rt reader's Q\n", 
								n_readers_to_be_service);

	rt_update_vector_count = rt_get_updated_rt_entries(rt, &rt_update_vector);// this msg is freed by last reader exiting CS
	printk(KERN_INFO "worker thread finds the no of entries to be delivered to each reader = %u, rt_update_vector = 0x%x\n", 
				rt_update_vector_count, (unsigned int)rt_update_vector);
	
	allow_rt_access_to_readers(rt, n_readers_to_be_service);
	
	printk(KERN_INFO "worker thread waiting for the wakeup signal from last reader\n");
	worker_thread->busy_mode = BUSY_WITH_READERS;
	printk(KERN_INFO "worker thread busy mode status = %s\n", get_str_busy_mode(get_kern_thread_busy_mode(worker_thread)));	
	wait_for_completion(&worker_thread->completion);

	printk(KERN_INFO "worker thread has recieved the wakeup signal from the last reader, woken up\n");
	worker_thread->busy_mode = IDLE;
	printk(KERN_INFO "worker thread busy mode status = %s\n", get_str_busy_mode(get_kern_thread_busy_mode(worker_thread)));
	goto WAIT_FOR_WRITER_ARRIVAL;
	return 0;
}


long ioctl_rt_handler1 (struct file *filep, unsigned int cmd, unsigned long arg){
	long rc = 0;
	struct kernthread *writer_thread = NULL,
			  *reader_thread = NULL,
		          *kern_thread   = NULL;

	struct rt_update_t kmsg;
	
	kern_thread = (struct kernthread *)filep->private_data;
	memset(&kmsg, 0, sizeof(struct rt_update_t));


	switch(cmd){
		case (RT_IOC_COMMON_UPDATE_RT):
			{
				writer_thread = kern_thread;
				if(access_ok(VERIFY_READ, (void __user*)arg, sizeof(struct rt_update_t)) ==  0){
					printk(KERN_INFO "%s() : invalid user space ptr\n", __FUNCTION__);
					return rc;
				}
				
				copy_from_user(&kmsg, (void __user *)arg, sizeof(struct rt_update_t));
				printk(KERN_INFO "writer process \"%s\" (pid %i) enters the system\n", 
						writer_thread->task->comm, writer_thread->task->pid);

				printk(KERN_INFO "writer thread \"%s\" (pid %i) is locking the writer Queue = 0x%x, writer Queue count = %u\n", 
						writer_thread->task->comm, writer_thread->task->pid, (unsigned int)rt->writer_Q, Q_COUNT(rt->writer_Q));

				SEM_LOCK_WRITER_Q(rt);
				enqueue(rt->writer_Q, writer_thread);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) is enqueued in writer Queue, writer Queue count = %u\n", 
						writer_thread->task->comm, writer_thread->task->pid, Q_COUNT(rt->writer_Q));
				SEM_UNLOCK_WRITER_Q(rt);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) has released thelock on writer Q\n", 
						writer_thread->task->comm, writer_thread->task->pid);

				printk(KERN_INFO "writer thread \"%s\" (pid %i) is blocking itself now to be scheduled by worker thread\n", writer_thread->task->comm, writer_thread->task->pid);
				wake_up_interruptible(&rt->writerQ);
				wait_for_completion(&writer_thread->completion);
				
				printk(KERN_INFO "writer thread \"%s\" (pid %i) resumes and granted access to rt\n", writer_thread->task->comm, writer_thread->task->pid);	
				apply_rt_updates(rt, &kmsg);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) has updated the rt\n", writer_thread->task->comm, writer_thread->task->pid);

				printk(KERN_INFO "writer thread \"%s\" (pid %i) invoking the worker thread now\n", writer_thread->task->comm, writer_thread->task->pid);
				complete(&rt->worker_thread->completion);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) jobs done, exiting the system\n",  writer_thread->task->comm, writer_thread->task->pid); 
			}
			break;
		case (RT_IOC_RTPURGE):
			{
				writer_thread = kern_thread;
				printk(KERN_INFO "writer process \"%s\" (pid %i) enters the system\n", 
						writer_thread->task->comm, writer_thread->task->pid);

				printk(KERN_INFO "writer thread \"%s\" (pid %i) is locking the writer Queue = 0x%x, writer Queue count = %u\n", 
						writer_thread->task->comm, writer_thread->task->pid, (unsigned int)rt->writer_Q, Q_COUNT(rt->writer_Q));

				SEM_LOCK_WRITER_Q(rt);
				enqueue(rt->writer_Q, writer_thread);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) is enqueued in writer Queue, writer Queue count = %u\n", 
						writer_thread->task->comm, writer_thread->task->pid, Q_COUNT(rt->writer_Q));
				SEM_UNLOCK_WRITER_Q(rt);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) has released thelock on writer Q\n", 
						writer_thread->task->comm, writer_thread->task->pid);

				printk(KERN_INFO "writer thread \"%s\" (pid %i) is blocking itself now to be scheduled by worker thread\n", writer_thread->task->comm, writer_thread->task->pid);
				wake_up_interruptible(&rt->writerQ);
				wait_for_completion(&writer_thread->completion);
				
				printk(KERN_INFO "writer thread \"%s\" (pid %i) resumes and granted access to rt\n", writer_thread->task->comm, writer_thread->task->pid);	
				purge_rt_table(rt);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) has purged  the rt\n", writer_thread->task->comm, writer_thread->task->pid);

				printk(KERN_INFO "writer thread \"%s\" (pid %i) invoking the worker thread now\n", writer_thread->task->comm, writer_thread->task->pid);
				complete(&rt->worker_thread->completion);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) jobs done, exiting the system\n",  writer_thread->task->comm, writer_thread->task->pid); 
			}
			break;
		case (RT_IOC_CR_RTENTRY):
			{
				struct rt_entry entry;
				writer_thread = kern_thread;
				copy_from_user((void *)&entry, (const void __user *)arg, sizeof(struct rt_entry));
				printk(KERN_INFO "writer process \"%s\" (pid %i) enters the system\n", 
						writer_thread->task->comm, writer_thread->task->pid);

				printk(KERN_INFO "writer thread \"%s\" (pid %i) is locking the writer Queue = 0x%x, writer Queue count = %u\n", 
						writer_thread->task->comm, writer_thread->task->pid, (unsigned int)rt->writer_Q, Q_COUNT(rt->writer_Q));

				SEM_LOCK_WRITER_Q(rt);
				enqueue(rt->writer_Q, writer_thread);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) is enqueued in writer Queue, writer Queue count = %u\n", 
						writer_thread->task->comm, writer_thread->task->pid, Q_COUNT(rt->writer_Q));
				SEM_UNLOCK_WRITER_Q(rt);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) has released thelock on writer Q\n", 
						writer_thread->task->comm, writer_thread->task->pid);

				printk(KERN_INFO "writer thread \"%s\" (pid %i) is blocking itself now to be scheduled by worker thread\n", writer_thread->task->comm, writer_thread->task->pid);
				wake_up_interruptible(&rt->writerQ);
				wait_for_completion(&writer_thread->completion);
				
				printk(KERN_INFO "writer thread \"%s\" (pid %i) resumes and granted access to rt\n", writer_thread->task->comm, writer_thread->task->pid);	
				
				add_rt_table_entry_by_val(rt, entry);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) has updated the rt\n", writer_thread->task->comm, writer_thread->task->pid);

				printk(KERN_INFO "writer thread \"%s\" (pid %i) invoking the worker thread now\n", writer_thread->task->comm, writer_thread->task->pid);
				complete(&rt->worker_thread->completion);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) jobs done, exiting the system\n",  writer_thread->task->comm, writer_thread->task->pid); 
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
				unsigned int actual_count = 0, counter = 0, update_count = 0;
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

				update_count = GET_RT_CHANGELIST_ENTRY_COUNT(rt);
				put_user(update_count, &(rt_info->no_of_pending_updates));

				up_read(&rt->rw_sem);
#if 0
				copy_to_user((void __user *)&(rt_info->actual_node_count), &actual_count, 
						sizeof(unsigned int));
#endif
				rc = 0;	
			}
			break;
		case (RT_IOC_SUBSCRIBE_RT):
			{
				reader_thread = kern_thread;
				printk(KERN_INFO "reader process \"%s\" (pid %i) enters the system\n",	
						reader_thread->task->comm, reader_thread->task->pid);
			
				if(access_ok(VERIFY_WRITE, (void __user*)arg, sizeof(struct rt_update_t) * MAX_ENTRIES_FETCH) ==  0){
					printk(KERN_INFO "%s() : invalid user space ptr\n", __FUNCTION__);
					return rc;
				}
			
				printk(KERN_INFO "No of rt updates to be read by reader thread \"%s\" (pid %i) = %u\n", 
					reader_thread->task->comm, reader_thread->task->pid, rt_update_vector_count);	

				if(rt_update_vector_count > MAX_ENTRIES_FETCH)
					rt_update_vector_count = MAX_ENTRIES_FETCH;

				printk(KERN_INFO "reader thread \"%s\" (pid %i) is locking the reader Queue = 0x%x, reader Queue count = %u\n",
                                                reader_thread->task->comm, reader_thread->task->pid, (unsigned int)rt->reader_Q, Q_COUNT(rt->reader_Q));

				SEM_LOCK_READER_Q(rt);
                                enqueue(rt->reader_Q, reader_thread);
                                printk(KERN_INFO "reader thread \"%s\" (pid %i) is enqueued in reader Queue, reader Queue count = %u\n",
                                                reader_thread->task->comm, reader_thread->task->pid, Q_COUNT(rt->reader_Q));
                                SEM_UNLOCK_READER_Q(rt);
                                printk(KERN_INFO "reader thread \"%s\" (pid %i) has released the lock on reader Q\n",
                                                reader_thread->task->comm, reader_thread->task->pid);
				printk(KERN_INFO "reader thread \"%s\" (pid %i) is blocking itself now to be scheduled by worker thread\n", reader_thread->task->comm, reader_thread->task->pid);
                                wait_for_completion(&reader_thread->completion);

				printk(KERN_INFO "reader thread \"%s\" (pid %i) wake up from sleep\n", reader_thread->task->comm, reader_thread->task->pid);
				
				/* Write a logic here for the reader thread to access the rt here*/
				/* CS Entry Begin*/
                                printk(KERN_INFO "reader thread \"%s\" (pid %i) is entering the CS\n", reader_thread->task->comm, reader_thread->task->pid);
				/* CS Entry ends*/

				/* Access CS BEGIN*/
				printk(KERN_INFO "reader thread \"%s\" (pid %i) is accessing the CS\n", reader_thread->task->comm, reader_thread->task->pid);
				copy_to_user((void __user *)arg, rt_update_vector, sizeof(struct rt_entry) * rt_update_vector_count);
				rc = rt_update_vector_count;
				/* Access CS ends*/
	
				/* EXIT CS BEGIN*/
				down_interruptible(&serialize_readers_cs_sem);
				if(n_readers_to_be_service == 1){
					printk(KERN_INFO "reader thread \"%s\" (pid %i) is the  last reader, freeing the rt update change list\n", reader_thread->task->comm, reader_thread->task->pid);
					rt_update_vector_count = 0;
					rt_empty_change_list(rt);
					kfree(rt_update_vector);
					rt_update_vector = NULL;
					printk(KERN_INFO "reader thread \"%s\" (pid %i) is the  last reader, sending signal to worker thread\n", reader_thread->task->comm, reader_thread->task->pid);
					complete(&rt->worker_thread->completion);
				}
				n_readers_to_be_service--;
				printk(KERN_INFO "reader thread \"%s\" (pid %i) leaves the CS\n", reader_thread->task->comm, reader_thread->task->pid);
				up(&serialize_readers_cs_sem);
				/* EXIT CS ENDS*/
				printk(KERN_INFO "reader thread \"%s\" (pid %i) finihed access to the rt and leaves the kernel space\n", reader_thread->task->comm, reader_thread->task->pid); 	
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

