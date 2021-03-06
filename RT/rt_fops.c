#include "rt_fops.h"
#include "rtkernusr.h"
#include "rt_table.h"
#include <asm/uaccess.h> // for datums
#include <linux/slab.h>
#include "../common/kernutils.h"
#include  <linux/poll.h>

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
	.poll		=     rt_poll,
};

/* internal functions*/

/* Global variables*/
static unsigned int n_readers_to_be_service = 0; 
static struct semaphore rt_serialize_readers_cs_sem;
static struct rt_update_t *rt_update_vector = NULL;
static int rt_update_vector_count = 0;
static struct ll_t *black_listed_poll_readers_list = NULL; // poll readers who attempt to read the same  update again


void rt_driver_init(void){
	sema_init(&rt_serialize_readers_cs_sem, 1);
	black_listed_poll_readers_list = init_singly_ll();
}

/* A list of black listers 
Note: A black lister is not a kernel thread, but identified by struct file *filep
*/

int rt_open (struct inode *inode, struct file *filp){

	struct kernthread *kernthread = NULL;
	kernthread = kzalloc(sizeof(struct kernthread), GFP_KERNEL);
	printk(KERN_INFO "%s() : %s\" (pid %i),  inode = %p, filep = %p\n", 
		__FUNCTION__, get_current()->comm, get_current()->pid, inode,   filp);
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
	
	printk(KERN_INFO "%s() is called , inode = %p, filep = %p\n", __FUNCTION__, inode,   filp);
	
	return 0;
}


ssize_t rt_read (struct file *filp, char __user *buf, size_t count, loff_t *f_pos){
	
	unsigned int isDeleted = 0, rc = 0;
	down_read(&rt->rw_sem);
	printk(KERN_INFO "%s() : poll reader %s\" (pid %i) enters \n", __FUNCTION__, get_current()->comm, get_current()->pid);
	printk(KERN_INFO "%s() poll reader %s\" (pid %i) is reading rt, no of polling readers to read the update = %u\n", __FUNCTION__, get_current()->comm, get_current()->pid, RT_GET_POLL_READER_COUNT(rt)); 

	/* CS entery begins */
	SEM_LOCK(&rt_serialize_readers_cs_sem);
		if(is_singly_ll_empty(black_listed_poll_readers_list)){
			 printk(KERN_INFO "%s() poll reader %s\" (pid %i) is the first reader\n",__FUNCTION__, get_current()->comm, get_current()->pid);
			 rt_update_vector_count = rt_get_updated_rt_entries(rt, &rt_update_vector); // this msg is freed by last reader exiting CS
                         printk(KERN_INFO "no of entries to be delivered to each reader = %u, rt_update_vector = %p\n", 
				rt_update_vector_count, rt_update_vector);
			if(rt_update_vector_count > RT_MAX_ENTRIES_FETCH)
					rt_update_vector_count = RT_MAX_ENTRIES_FETCH;
			rc = rt_update_vector_count;
		}
	SEM_UNLOCK(&rt_serialize_readers_cs_sem);
	/* CS entery ends */

	/* CS begins*/
	copy_to_user((void __user *)buf, rt_update_vector, sizeof(struct rt_entry) * rt_update_vector_count);
	rc = rt_update_vector_count;
 	/* CS ends*/

	SEM_LOCK(&rt_serialize_readers_cs_sem);

	if(RT_GET_POLL_READER_COUNT(rt) == 1){
		printk(KERN_INFO "%s() last poll reader \"%s\" (pid %i) has also read the rt update\n", __FUNCTION__, get_current()->comm, get_current()->pid);
		printk(KERN_INFO "%s() last poll reader \"%s\" (pid %i) has deleted the change list\n", __FUNCTION__, get_current()->comm, get_current()->pid);	
		rt_empty_change_list(rt);
		rc = rt_update_vector_count;
		rt_update_vector_count = 0;
		kfree(rt_update_vector);
	}	

	if(NULL == singly_ll_is_value_present(rt->poll_readers_list, &filp, sizeof(struct filep **))){
		printk(KERN_INFO "%s() Error : poll reader \"%s\" (pid %i) , filep = %p, is not present in rt->poll_readers_list\n", __FUNCTION__, get_current()->comm, get_current()->pid, filp);
		print_singly_LL(rt->poll_readers_list);
	}
	else{
		printk(KERN_INFO "%s() deleting \"%s\" (pid %i) : from rt->poll_readers_list\n", __FUNCTION__, get_current()->comm, get_current()->pid);
		isDeleted = singly_ll_delete_node_by_value(rt->poll_readers_list, (void *)&filp, sizeof(struct filep **));
	}
	printk(KERN_INFO "%s() poll reader \"%s\" (pid %i) has read the data, removed from orig poll readers list, isDeleted = %u\n", __FUNCTION__, get_current()->comm, get_current()->pid, isDeleted);
	singly_ll_add_node_by_val(black_listed_poll_readers_list, (void *)&filp, sizeof(struct filep **));
	printk(KERN_INFO "%s() poll reader \"%s\" (pid %i) has read the data, adding to black lister's list\n", __FUNCTION__, get_current()->comm, get_current()->pid);
	
	SEM_UNLOCK(&rt_serialize_readers_cs_sem);
	up_read(&rt->rw_sem);
	return rc;
}


ssize_t rt_write (struct file *filp, const char __user *buf, size_t count, loff_t *f_pos){

	struct rt_update_t kmsg;
	printk(KERN_INFO "%s() is called , filep = %p, user buff = %p, buff_size = %ld\n",
			__FUNCTION__,  filp,  buf, count);


	memset(&kmsg, 0, sizeof(struct rt_update_t));

	if(access_ok((void __user*)buf, count) ==  0){
		printk(KERN_INFO "%s() : invalid user space ptr\n", __FUNCTION__);
		return 0;
	}

	copy_from_user(&kmsg, (void __user *)buf, sizeof(struct rt_update_t));

	/* down_write/up_write is same as RT_LOCK_SEM(rt)/RT_UNLOCK_SEM(rt) in write case*/
	down_write(&rt->rw_sem); //RT_LOCK_SEM(rt);
	apply_rt_updates(rt, &kmsg);
	up_write(&rt->rw_sem); //RT_UNLOCK_SEM(rt);
	printk(KERN_INFO "%s() \"%s\" sending wake_up call to rt->readerQ\n", __FUNCTION__, get_current()->comm);
	wake_up(&rt->readerQ); // wake up the polling reader processes, poll fn is called again
	
	return sizeof(struct rt_update_t);
}

static int
allow_rt_access_to_readers(struct rt_table *rt, unsigned int n){
/* fn to allow n readers from rt reader q to allow rt access */
   unsigned int i = 0;
   struct kernthread *reader_thread = NULL;

   printk(KERN_INFO "inside %s() ....\n", __FUNCTION__);
	
   for (; i < n; i++){
	RT_SEM_LOCK_READER_Q(rt);
	reader_thread = deque(rt->reader_Q);
	RT_SEM_UNLOCK_READER_Q(rt);
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
	
	printk(KERN_INFO "worker thread is locking the writer Queue= %p\n", rt->writer_Q);
	RT_SEM_LOCK_WRITER_Q(rt);
	
	writer_thread = (struct kernthread *)deque(rt->writer_Q);
	printk(KERN_INFO "worker thread has extracted the writer \"%s\" (pid %i) from writer Queue = %p\n", 
			writer_thread->task->comm, writer_thread->task->pid, rt->writer_Q);

	RT_SEM_UNLOCK_WRITER_Q(rt);
	printk(KERN_INFO "worker thread has released locked over writer Queue= %p\n", rt->writer_Q);
	
	if(!writer_thread){
		printk(KERN_INFO "worker thread finds there is no writer in writer_Q, worker thread is sleeping\n");
		RT_SEM_UNLOCK_WRITER_Q(rt);
		printk(KERN_INFO "worker thread has unlocked the writer Queue= %p\n", rt->writer_Q);
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

	RT_SEM_LOCK_READER_Q(rt);
	n_readers_to_be_service = Q_COUNT(rt->reader_Q);
	RT_SEM_UNLOCK_READER_Q(rt);	
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
	printk(KERN_INFO "worker thread finds the no of entries to be delivered to each reader = %u, rt_update_vector = %p\n", 
				rt_update_vector_count, rt_update_vector);
	
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
				if(access_ok((void __user*)arg, sizeof(struct rt_update_t)) ==  0){
					printk(KERN_INFO "%s() : invalid user space ptr\n", __FUNCTION__);
					return rc;
				}
				
				copy_from_user(&kmsg, (void __user *)arg, sizeof(struct rt_update_t));
				printk(KERN_INFO "writer process \"%s\" (pid %i) enters the system\n", 
						writer_thread->task->comm, writer_thread->task->pid);

				printk(KERN_INFO "writer thread \"%s\" (pid %i) is locking the writer Queue = %p, writer Queue count = %u\n", 
						writer_thread->task->comm, writer_thread->task->pid, rt->writer_Q, Q_COUNT(rt->writer_Q));

				RT_SEM_LOCK_WRITER_Q(rt);
				enqueue(rt->writer_Q, writer_thread);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) is enqueued in writer Queue, writer Queue count = %u\n", 
						writer_thread->task->comm, writer_thread->task->pid, Q_COUNT(rt->writer_Q));
				RT_SEM_UNLOCK_WRITER_Q(rt);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) has released thelock on writer Q\n", 
						writer_thread->task->comm, writer_thread->task->pid);

				printk(KERN_INFO "writer thread \"%s\" (pid %i) is blocking itself now to be scheduled by worker thread\n", writer_thread->task->comm, writer_thread->task->pid);
				wake_up_interruptible(&rt->writerQ);
				wait_for_completion(&writer_thread->completion);
				
				printk(KERN_INFO "writer thread \"%s\" (pid %i) resumes and granted access to rt\n", writer_thread->task->comm, writer_thread->task->pid);
				/* we are locking it because, during polling, the table status could be queried*/
				RT_LOCK_SEM(rt);	
				apply_rt_updates(rt, &kmsg);
				RT_UNLOCK_SEM(rt);
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

				printk(KERN_INFO "writer thread \"%s\" (pid %i) is locking the writer Queue = %p, writer Queue count = %u\n", 
						writer_thread->task->comm, writer_thread->task->pid, rt->writer_Q, Q_COUNT(rt->writer_Q));

				RT_SEM_LOCK_WRITER_Q(rt);
				enqueue(rt->writer_Q, writer_thread);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) is enqueued in writer Queue, writer Queue count = %u\n", 
						writer_thread->task->comm, writer_thread->task->pid, Q_COUNT(rt->writer_Q));
				RT_SEM_UNLOCK_WRITER_Q(rt);
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

				printk(KERN_INFO "writer thread \"%s\" (pid %i) is locking the writer Queue = %p, writer Queue count = %u\n", 
						writer_thread->task->comm, writer_thread->task->pid, rt->writer_Q, Q_COUNT(rt->writer_Q));

				RT_SEM_LOCK_WRITER_Q(rt);
				enqueue(rt->writer_Q, writer_thread);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) is enqueued in writer Queue, writer Queue count = %u\n", 
						writer_thread->task->comm, writer_thread->task->pid, Q_COUNT(rt->writer_Q));
				RT_SEM_UNLOCK_WRITER_Q(rt);
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

				if(access_ok((void __user*)arg, sizeof(struct rt_info_t)) ==  0){
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

				counter = RT_GET_POLL_READER_COUNT(rt);
				put_user(counter, &(rt_info->no_of_polling_readers));
				
				counter = GET_NODE_COUNT_SINGLY_LL(black_listed_poll_readers_list);
				put_user(counter, &(rt_info->no_of_blacklisted_polling_readers));

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
			
				if(access_ok((void __user*)arg, sizeof(struct rt_update_t) * RT_MAX_ENTRIES_FETCH) ==  0){
					printk(KERN_INFO "%s() : invalid user space ptr\n", __FUNCTION__);
					return rc;
				}
			
				printk(KERN_INFO "No of rt updates to be read by reader thread \"%s\" (pid %i) = %u\n", 
					reader_thread->task->comm, reader_thread->task->pid, rt_update_vector_count);	

				if(rt_update_vector_count > RT_MAX_ENTRIES_FETCH)
					rt_update_vector_count = RT_MAX_ENTRIES_FETCH;

				printk(KERN_INFO "reader thread \"%s\" (pid %i) is locking the reader Queue = %p, reader Queue count = %u\n",
                                                reader_thread->task->comm, reader_thread->task->pid, rt->reader_Q, Q_COUNT(rt->reader_Q));

				RT_SEM_LOCK_READER_Q(rt);
                                enqueue(rt->reader_Q, reader_thread);
                                printk(KERN_INFO "reader thread \"%s\" (pid %i) is enqueued in reader Queue, reader Queue count = %u\n",
                                                reader_thread->task->comm, reader_thread->task->pid, Q_COUNT(rt->reader_Q));
                                RT_SEM_UNLOCK_READER_Q(rt);
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
				down_interruptible(&rt_serialize_readers_cs_sem);
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
				up(&rt_serialize_readers_cs_sem);
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
	printk(KERN_INFO "%s() is called , filep = %p, cmd code = %d\n", __FUNCTION__,  filep, cmd);
	return 0;
}

unsigned int rt_poll (struct file *filep, struct poll_table_struct *poll_table){

#define WAIT_FOR_DATA_AVAILABILITY	0
#define WAIT_FOR_DATA_WRITE		0

	unsigned int isDataAvailable = 0; // 0 not, >0 means yes
	
	/* if data is not available then choose one out of  two actions. refer rules on page 166, LDD3
	   1. if O_NONBLOCK flag is set, then return immediately with return value -EAGAIN
	   2. if O_NONBLOCK flag is not set, then block untill atleast one byte of data is available
	   return 0;
	 */

	/* lock the rt table, hence, in case if writers writing into rt, rt should be locked by sem*/
	/* Add unique file pointers to the rt->poll_readers_list to keep trackof unique poll readers
	  if file desc is already present, then no op
	*/

	 SEM_LOCK(&rt_serialize_readers_cs_sem);
	 if (NULL == singly_ll_is_value_present(black_listed_poll_readers_list, &filep, sizeof(struct file **))){
		printk(KERN_INFO "%s() poll reader %s\" (pid %i) is not black listed\n", __FUNCTION__, get_current()->comm, get_current()->pid);
		isDataAvailable = mutex_is_rt_updated(rt); // This operation need not be mutex protected, redundant now
		if(isDataAvailable){
			 printk(KERN_INFO "%s() poll reader %s\" (pid %i) finds data is Available, returning POLLIN|POLLRDNORM\n", __FUNCTION__, get_current()->comm, get_current()->pid);
	 		 SEM_UNLOCK(&rt_serialize_readers_cs_sem);
			 return POLLIN|POLLRDNORM;
		}
		else{
			/* case 1 : if i am not black listed and data not available*/
			printk(KERN_INFO "%s() poll reader %s\" (pid %i) finds data is not Available, means i am fresh entry into system\n", __FUNCTION__, get_current()->comm, get_current()->pid);
			if(IS_FILE_ACCESS_FLAG_SET(filep, O_NONBLOCK)){
				SEM_UNLOCK(&rt_serialize_readers_cs_sem);
				return -EAGAIN; // select return 2 in user space corresponding to this
			}
			add_rt_table_unique_poll_reader(rt, filep);
			printk(KERN_INFO "%s() poll reader %s\" (pid %i) added to rt orig polling list\n", __FUNCTION__, get_current()->comm, get_current()->pid);
			poll_wait(filep, &rt->readerQ, poll_table);
			printk(KERN_INFO "%s() poll reader %s\" (pid %i) is blocked for Data availablity\n", __FUNCTION__, get_current()->comm, get_current()->pid);
			SEM_UNLOCK(&rt_serialize_readers_cs_sem);
			return WAIT_FOR_DATA_AVAILABILITY;
		}
	}

	else{ /* if i am black listed*/
		printk(KERN_INFO "%s() poll reader %s\" (pid %i) is black listed\n", __FUNCTION__, get_current()->comm, get_current()->pid);
		isDataAvailable = mutex_is_rt_updated(rt);
		if(isDataAvailable){
			/* Black list and data is available */
			printk(KERN_INFO "%s() poll reader %s\" (pid %i) finds data is Available\n", __FUNCTION__, get_current()->comm, get_current()->pid);
			poll_wait(filep, &rt->readerQ, poll_table);
			printk(KERN_INFO "%s() poll reader %s\" (pid %i) has already  read this update, blocking itself\n", __FUNCTION__, get_current()->comm, get_current()->pid);
			SEM_UNLOCK(&rt_serialize_readers_cs_sem);
			return WAIT_FOR_DATA_AVAILABILITY;
		}
		else{ /* Blacklisted but data is not available*/
			printk(KERN_INFO "%s() poll reader %s\" (pid %i) finds data is not Available\n", __FUNCTION__, get_current()->comm, get_current()->pid);
			printk(KERN_INFO "%s() poll reader %s\" (pid %i) is probably the last reader\n", __FUNCTION__, get_current()->comm, get_current()->pid);
			if(RT_GET_POLL_READER_COUNT(rt) == 0){	
				printk(KERN_INFO "%s() rt->poll_readers_list is empty, copying the blacklist list into rt->poll_readers_list\n", __FUNCTION__);

				/* Do not copy this way, if some new reader comes while old readers have still not yet finished reading the  update,
				   we may end up in losing the new reader. Copy nodes from black_listed_poll_readers_list to rt->poll_readers_list instead
				   TBD */
				rt->poll_readers_list->head = black_listed_poll_readers_list->head;
				rt->poll_readers_list->node_count = black_listed_poll_readers_list->node_count;
				black_listed_poll_readers_list->head = NULL;
				black_listed_poll_readers_list->node_count = 0; 
			}
			else{
				printk(KERN_INFO "%s() Error : rt->poll_readers_list is not empty !! Count = %d\n", __FUNCTION__, RT_GET_POLL_READER_COUNT(rt)); 
			}
			printk(KERN_INFO "%s() black_listed_poll_readers_list count = %d, rt->poll_readers_list_count = %d seen by %s\" (pid %i), and is blocking itself\n", 
						__FUNCTION__, black_listed_poll_readers_list->node_count, rt->poll_readers_list->node_count, get_current()->comm, get_current()->pid);
			poll_wait(filep, &rt->readerQ, poll_table);
			SEM_UNLOCK(&rt_serialize_readers_cs_sem);
			return WAIT_FOR_DATA_AVAILABILITY;
		}
	}	
}
