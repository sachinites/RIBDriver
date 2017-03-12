#include "mac_fops.h"
#include "mackernusr.h"
#include "mac_table.h"
#include <asm/uaccess.h> // for datums
#include <linux/slab.h>
#include "../common/kernutils.h"

extern struct mac_table *mac;

struct file_operations mac_fops = {
	.owner 		=     THIS_MODULE,
	.llseek 	=     NULL, 
	.read 		=     mac_read,
	.write 		=     mac_write,
	.unlocked_ioctl =     NULL, 
	.open 		=     mac_open,
	.release 	=     mac_release,
	.unlocked_ioctl =     ioctl_mac_handler1,
	.compat_ioctl   =     ioctl_mac_handler2,
};

/* internal functions*/

/* Global variables*/
static unsigned int n_readers_to_be_service = 0; 
struct semaphore mac_serialize_readers_cs_sem;
static struct mac_update_t *mac_update_vector = NULL;
static int mac_update_vector_count = 0;

int mac_open (struct inode *inode, struct file *filp){

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

int mac_release (struct inode *inode, struct file *filp){
	
	printk(KERN_INFO "%s() is called , inode = 0x%x, filep = 0x%x\n", __FUNCTION__, (unsigned int)inode,  (unsigned int) filp);
	
	return 0;
}


ssize_t mac_read (struct file *filp, char __user *buf, size_t count, loff_t *f_pos){
	
	ssize_t n_count = 0;
	
	down_read(&mac->rw_sem);
	n_count = copy_mac_table_to_user_space(mac, buf, count);
	up_read(&mac->rw_sem);

	return n_count;
}


ssize_t mac_write (struct file *filp, const char __user *buf, size_t count, loff_t *f_pos){

	printk(KERN_INFO "%s() is called , filep = 0x%x, user buff = 0x%x, buff_size = %d\n",
			__FUNCTION__, (unsigned int) filp, (unsigned int) buf, count);
	return 0;
}

static int
allow_mac_access_to_readers(struct mac_table *mac, unsigned int n){
/* fn to allow n readers from mac reader q to allow mac access */
   unsigned int i = 0;
   struct kernthread *reader_thread = NULL;

   printk(KERN_INFO "inside %s() ....\n", __FUNCTION__);
	
   for (; i < n; i++){
	MAC_SEM_LOCK_READER_Q(mac);
	reader_thread = deque(mac->reader_Q);
	MAC_SEM_UNLOCK_READER_Q(mac);
	printk(KERN_INFO "giving mac access to reader thread : \"%s\" (pid %i)\n", 
			 reader_thread->task->comm, reader_thread->task->pid);	
	complete(&reader_thread->completion);
   }
   return 0;
}



int mac_worker_fn(void *arg){
	struct kernthread *worker_thread = NULL,
			  *writer_thread = NULL;

	printk(KERN_INFO "%s() is called is and activated. Waiting for the writer's arrival\n", __FUNCTION__);

	worker_thread = (struct kernthread * )arg;
	
	WAIT_FOR_WRITER_ARRIVAL:
	printk(KERN_INFO "worker thread waiting for writer's arrival Or scheduling the next writer\n");
	wait_event_interruptible(mac->writerQ, !(is_queue_empty(mac->writer_Q)));
	printk(KERN_INFO "worker thread is revived from sleep, picking up the first writer thread from writerQ\n");
	
	printk(KERN_INFO "worker thread is locking the writer Queue= 0x%x\n", (unsigned int)mac->writer_Q);
	MAC_SEM_LOCK_WRITER_Q(mac);
	
	writer_thread = (struct kernthread *)deque(mac->writer_Q);
	printk(KERN_INFO "worker thread has extracted the writer \"%s\" (pid %i) from writer Queue = 0x%x\n", 
			writer_thread->task->comm, writer_thread->task->pid, (unsigned int)mac->writer_Q);

	MAC_SEM_UNLOCK_WRITER_Q(mac);
	printk(KERN_INFO "worker thread has released locked over writer Queue= 0x%x\n", (unsigned int)mac->writer_Q);
	
	if(!writer_thread){
		printk(KERN_INFO "worker thread finds there is no writer in writer_Q, worker thread is sleeping\n");
		MAC_SEM_UNLOCK_WRITER_Q(mac);
		printk(KERN_INFO "worker thread has unlocked the writer Queue= 0x%x\n", (unsigned int)mac->writer_Q);
		goto WAIT_FOR_WRITER_ARRIVAL;
	}

	printk(KERN_INFO "worker thread signalling (completion) writer \"%s\" (pid %i) to access mac\n", 
			writer_thread->task->comm, writer_thread->task->pid);
	complete(&writer_thread->completion);
	printk(KERN_INFO "worker thread waiting for writer thread to finish with mac, calling wait_for_completion\n");
	worker_thread->busy_mode = BUSY_WITH_WRITERS;
	printk(KERN_INFO "worker thread busy mode status = %s\n", get_str_busy_mode(get_kern_thread_busy_mode(worker_thread)));
	wait_for_completion(&worker_thread->completion);
	printk(KERN_INFO "worker thread has recieved the wakeup signal from writer thread, woken up \n");
	printk(KERN_INFO "worker thread is calculating the no of readers in reader_q now\n");
	MAC_SEM_LOCK_READER_Q(mac);
	n_readers_to_be_service = Q_COUNT(mac->reader_Q);
	MAC_SEM_UNLOCK_READER_Q(mac);	
	printk(KERN_INFO "worker thread finds the no. of readers to be serviced  = %u\n", n_readers_to_be_service);

	if(n_readers_to_be_service == 0){
		printk(KERN_INFO "worker thread finds there are no readers\n");
		worker_thread->busy_mode = IDLE;
		/* delete all updates in mac change list as their are no readers*/
		mac_empty_change_list(mac);	
		goto WAIT_FOR_WRITER_ARRIVAL;
	}

	printk(KERN_INFO "worker thread is preparing update msg to service %u readers from mac reader's Q\n", 
								n_readers_to_be_service);

	mac_update_vector_count = mac_get_updated_mac_entries(mac, &mac_update_vector);// this msg is freed by last reader exiting CS
	printk(KERN_INFO "worker thread finds the no of entries to be delivered to each reader = %u, mac_update_vector = 0x%x\n", 
				mac_update_vector_count, (unsigned int)mac_update_vector);
	
	allow_mac_access_to_readers(mac, n_readers_to_be_service);
	
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


long ioctl_mac_handler1 (struct file *filep, unsigned int cmd, unsigned long arg){
	long rc = 0;
	struct kernthread *writer_thread = NULL,
			  *reader_thread = NULL,
		          *kern_thread   = NULL;

	struct mac_update_t kmsg;
	
	kern_thread = (struct kernthread *)filep->private_data;
	memset(&kmsg, 0, sizeof(struct mac_update_t));


	switch(cmd){
		case (MAC_IOC_COMMON_UPDATE_MAC):
			{
				writer_thread = kern_thread;
				if(access_ok(VERIFY_READ, (void __user*)arg, sizeof(struct mac_update_t)) ==  0){
					printk(KERN_INFO "%s() : invalid user space ptr\n", __FUNCTION__);
					return rc;
				}
				
				copy_from_user(&kmsg, (void __user *)arg, sizeof(struct mac_update_t));
				printk(KERN_INFO "writer process \"%s\" (pid %i) enters the system\n", 
						writer_thread->task->comm, writer_thread->task->pid);

				printk(KERN_INFO "writer thread \"%s\" (pid %i) is locking the writer Queue = 0x%x, writer Queue count = %u\n", 
						writer_thread->task->comm, writer_thread->task->pid, (unsigned int)mac->writer_Q, Q_COUNT(mac->writer_Q));

				MAC_SEM_LOCK_WRITER_Q(mac);
				enqueue(mac->writer_Q, writer_thread);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) is enqueued in writer Queue, writer Queue count = %u\n", 
						writer_thread->task->comm, writer_thread->task->pid, Q_COUNT(mac->writer_Q));
				MAC_SEM_UNLOCK_WRITER_Q(mac);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) has released thelock on writer Q\n", 
						writer_thread->task->comm, writer_thread->task->pid);

				printk(KERN_INFO "writer thread \"%s\" (pid %i) is blocking itself now to be scheduled by worker thread\n", writer_thread->task->comm, writer_thread->task->pid);
				wake_up_interruptible(&mac->writerQ);
				wait_for_completion(&writer_thread->completion);
				
				printk(KERN_INFO "writer thread \"%s\" (pid %i) resumes and granted access to mac\n", writer_thread->task->comm, writer_thread->task->pid);	
				apply_mac_updates(mac, &kmsg);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) has updated the mac\n", writer_thread->task->comm, writer_thread->task->pid);

				printk(KERN_INFO "writer thread \"%s\" (pid %i) invoking the worker thread now\n", writer_thread->task->comm, writer_thread->task->pid);
				complete(&mac->worker_thread->completion);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) jobs done, exiting the system\n",  writer_thread->task->comm, writer_thread->task->pid); 
			}
			break;
		case (MAC_IOC_MACPURGE):
			{
				writer_thread = kern_thread;
				printk(KERN_INFO "writer process \"%s\" (pid %i) enters the system\n", 
						writer_thread->task->comm, writer_thread->task->pid);

				printk(KERN_INFO "writer thread \"%s\" (pid %i) is locking the writer Queue = 0x%x, writer Queue count = %u\n", 
						writer_thread->task->comm, writer_thread->task->pid, (unsigned int)mac->writer_Q, Q_COUNT(mac->writer_Q));

				MAC_SEM_LOCK_WRITER_Q(mac);
				enqueue(mac->writer_Q, writer_thread);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) is enqueued in writer Queue, writer Queue count = %u\n", 
						writer_thread->task->comm, writer_thread->task->pid, Q_COUNT(mac->writer_Q));
				MAC_SEM_UNLOCK_WRITER_Q(mac);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) has released thelock on writer Q\n", 
						writer_thread->task->comm, writer_thread->task->pid);

				printk(KERN_INFO "writer thread \"%s\" (pid %i) is blocking itself now to be scheduled by worker thread\n", writer_thread->task->comm, writer_thread->task->pid);
				wake_up_interruptible(&mac->writerQ);
				wait_for_completion(&writer_thread->completion);
				
				printk(KERN_INFO "writer thread \"%s\" (pid %i) resumes and granted access to mac\n", writer_thread->task->comm, writer_thread->task->pid);	
				purge_mac_table(mac);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) has purged  the mac\n", writer_thread->task->comm, writer_thread->task->pid);

				printk(KERN_INFO "writer thread \"%s\" (pid %i) invoking the worker thread now\n", writer_thread->task->comm, writer_thread->task->pid);
				complete(&mac->worker_thread->completion);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) jobs done, exiting the system\n",  writer_thread->task->comm, writer_thread->task->pid); 
			}
			break;
		case (MAC_IOC_CR_MACENTRY):
			{
				struct mac_entry entry;
				writer_thread = kern_thread;
				copy_from_user((void *)&entry, (const void __user *)arg, sizeof(struct mac_entry));
				printk(KERN_INFO "writer process \"%s\" (pid %i) enters the system\n", 
						writer_thread->task->comm, writer_thread->task->pid);

				printk(KERN_INFO "writer thread \"%s\" (pid %i) is locking the writer Queue = 0x%x, writer Queue count = %u\n", 
						writer_thread->task->comm, writer_thread->task->pid, (unsigned int)mac->writer_Q, Q_COUNT(mac->writer_Q));

				MAC_SEM_LOCK_WRITER_Q(mac);
				enqueue(mac->writer_Q, writer_thread);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) is enqueued in writer Queue, writer Queue count = %u\n", 
						writer_thread->task->comm, writer_thread->task->pid, Q_COUNT(mac->writer_Q));
				MAC_SEM_UNLOCK_WRITER_Q(mac);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) has released thelock on writer Q\n", 
						writer_thread->task->comm, writer_thread->task->pid);

				printk(KERN_INFO "writer thread \"%s\" (pid %i) is blocking itself now to be scheduled by worker thread\n", writer_thread->task->comm, writer_thread->task->pid);
				wake_up_interruptible(&mac->writerQ);
				wait_for_completion(&writer_thread->completion);
				
				printk(KERN_INFO "writer thread \"%s\" (pid %i) resumes and granted access to mac\n", writer_thread->task->comm, writer_thread->task->pid);	
				
				add_mac_table_entry_by_val(mac, entry);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) has updated the mac\n", writer_thread->task->comm, writer_thread->task->pid);

				printk(KERN_INFO "writer thread \"%s\" (pid %i) invoking the worker thread now\n", writer_thread->task->comm, writer_thread->task->pid);
				complete(&mac->worker_thread->completion);
				printk(KERN_INFO "writer thread \"%s\" (pid %i) jobs done, exiting the system\n",  writer_thread->task->comm, writer_thread->task->pid); 
			}
			break;
		case (MAC_IOC_D_MACENTRY):
			break;
		case (MAC_IOC_U_MACENTRY):
			break;
		case (MAC_IOC_LOOKUP_MACENTRY):
			break;
		case (MAC_IOC_GETMAC):
			break;
		case (MAC_IOC_MACDESTROY):
			break;
		case (MAC_IOC_MACOPEN):
			break;
		case (MAC_IOC_MACCLOSE):
			break;
		case (MAC_IOC_SET_ACCESS_MODE):
			break;
		case (MAC_IOC_GET_MAC_INFO):
			{
				struct mac_info_t *mac_info = (struct mac_info_t *)arg;
				unsigned int actual_count = 0, counter = 0, update_count = 0;
				struct singly_ll_node_t *head = GET_FIRST_MAC_ENTRY_NODE(mac);

				if(access_ok(VERIFY_WRITE, (void __user*)arg, sizeof(struct mac_info_t)) ==  0){
					printk(KERN_INFO "%s() : invalid user space ptr\n", __FUNCTION__);
					return rc;
				}
				
				down_read(&mac->rw_sem);
				counter = GET_MAC_ENTRY_COUNT(mac);
				copy_to_user((void __user *)&(mac_info->node_count), &counter, sizeof(unsigned int));
	
				while(head){
					actual_count++;
					head = GET_MAC_NXT_ENTRY_NODE(head);
				}	

				// copying using datums	
				put_user(actual_count, &(mac_info->actual_node_count));

				update_count = GET_MAC_CHANGELIST_ENTRY_COUNT(mac);
				put_user(update_count, &(mac_info->no_of_pending_updates));

				up_read(&mac->rw_sem);
#if 0
				copy_to_user((void __user *)&(mac_info->actual_node_count), &actual_count, 
						sizeof(unsigned int));
#endif
				rc = 0;	
			}
			break;
		case (MAC_IOC_SUBSCRIBE_MAC):
			{
				reader_thread = kern_thread;
				printk(KERN_INFO "reader process \"%s\" (pid %i) enters the system\n",	
						reader_thread->task->comm, reader_thread->task->pid);
			
				if(access_ok(VERIFY_WRITE, (void __user*)arg, sizeof(struct mac_update_t) * MAX_ENTRIES_FETCH) ==  0){
					printk(KERN_INFO "%s() : invalid user space ptr\n", __FUNCTION__);
					return rc;
				}
			
				printk(KERN_INFO "No of mac updates to be read by reader thread \"%s\" (pid %i) = %u\n", 
					reader_thread->task->comm, reader_thread->task->pid, mac_update_vector_count);	

				if(mac_update_vector_count > MAX_ENTRIES_FETCH)
					mac_update_vector_count = MAX_ENTRIES_FETCH;

				printk(KERN_INFO "reader thread \"%s\" (pid %i) is locking the reader Queue = 0x%x, reader Queue count = %u\n",
                                                reader_thread->task->comm, reader_thread->task->pid, (unsigned int)mac->reader_Q, Q_COUNT(mac->reader_Q));

				MAC_SEM_LOCK_READER_Q(mac);
                                enqueue(mac->reader_Q, reader_thread);
                                printk(KERN_INFO "reader thread \"%s\" (pid %i) is enqueued in reader Queue, reader Queue count = %u\n",
                                                reader_thread->task->comm, reader_thread->task->pid, Q_COUNT(mac->reader_Q));
                                MAC_SEM_UNLOCK_READER_Q(mac);
                                printk(KERN_INFO "reader thread \"%s\" (pid %i) has released the lock on reader Q\n",
                                                reader_thread->task->comm, reader_thread->task->pid);
				printk(KERN_INFO "reader thread \"%s\" (pid %i) is blocking itself now to be scheduled by worker thread\n", reader_thread->task->comm, reader_thread->task->pid);
                                wait_for_completion(&reader_thread->completion);

				printk(KERN_INFO "reader thread \"%s\" (pid %i) wake up from sleep\n", reader_thread->task->comm, reader_thread->task->pid);
				
				/* Write a logic here for the reader thread to access the mac here*/
				/* CS Entry Begin*/
                                printk(KERN_INFO "reader thread \"%s\" (pid %i) is entering the CS\n", reader_thread->task->comm, reader_thread->task->pid);
				/* CS Entry ends*/

				/* Access CS BEGIN*/
				printk(KERN_INFO "reader thread \"%s\" (pid %i) is accessing the CS\n", reader_thread->task->comm, reader_thread->task->pid);
				copy_to_user((void __user *)arg, mac_update_vector, sizeof(struct mac_entry) * mac_update_vector_count);
				rc = mac_update_vector_count;
				/* Access CS ends*/
	
				/* EXIT CS BEGIN*/
				down_interruptible(&mac_serialize_readers_cs_sem);
				if(n_readers_to_be_service == 1){
					printk(KERN_INFO "reader thread \"%s\" (pid %i) is the  last reader, freeing the mac update change list\n", reader_thread->task->comm, reader_thread->task->pid);
					mac_update_vector_count = 0;
					mac_empty_change_list(mac);
					kfree(mac_update_vector);
					mac_update_vector = NULL;
					printk(KERN_INFO "reader thread \"%s\" (pid %i) is the  last reader, sending signal to worker thread\n", reader_thread->task->comm, reader_thread->task->pid);
					complete(&mac->worker_thread->completion);
				}
				n_readers_to_be_service--;
				printk(KERN_INFO "reader thread \"%s\" (pid %i) leaves the CS\n", reader_thread->task->comm, reader_thread->task->pid);
				up(&mac_serialize_readers_cs_sem);
				/* EXIT CS ENDS*/
				printk(KERN_INFO "reader thread \"%s\" (pid %i) finihed access to the mac and leaves the kernel space\n", reader_thread->task->comm, reader_thread->task->pid); 	
			}
			break;
		default:
			printk(KERN_INFO "No ioctl defined\n");
			return -ENOTTY;

	}
	return rc;
}


long ioctl_mac_handler2 (struct file *filep, unsigned int cmd, unsigned long arg){
	printk(KERN_INFO "%s() is called , filep = 0x%x, cmd code = %d\n", __FUNCTION__, (unsigned int) filep, cmd);
	return 0;
}
