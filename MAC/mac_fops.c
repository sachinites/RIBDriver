#include "mac_fops.h"
#include "mackernusr.h"
#include "mac_table.h"
#include <asm/uaccess.h> // for datums
#include <linux/slab.h>
#include "../common/kernutils.h"
#include  <linux/poll.h>

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
	.poll		=     mac_poll,
};

/* internal functions*/

/* Global variables*/
static unsigned int n_readers_to_be_service = 0; 
static struct semaphore mac_serialize_readers_cs_sem;
static struct mac_update_t *mac_update_vector = NULL;
static int mac_update_vector_count = 0;
static struct ll_t *black_listed_poll_readers_list = NULL; // poll readers who attempt to read the same  update again


void mac_driver_init(void){
	sema_init(&mac_serialize_readers_cs_sem, 1);
	black_listed_poll_readers_list = init_singly_ll();
}


/* A list of black listers
   Note: A black lister is not a kernel thread, but identified by struct file *filep
*/


int mac_open (struct inode *inode, struct file *filp){

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

int mac_release (struct inode *inode, struct file *filp){
	
	printk(KERN_INFO "%s() is called , inode = %p, filep = %p\n", __FUNCTION__, inode,   filp);
	
	return 0;
}


ssize_t mac_read (struct file *filp, char __user *buf, size_t count, loff_t *f_pos){
	
	unsigned int isDeleted = 0, rc = 0;
        down_read(&mac->rw_sem);
        printk(KERN_INFO "%s() : poll reader %s\" (pid %i) enters \n", __FUNCTION__, get_current()->comm, get_current()->pid);
        printk(KERN_INFO "%s() poll reader %s\" (pid %i) is reading mac, no of polling readers to read the update = %u\n", __FUNCTION__, get_current()->comm, get_current()->pid, MAC_GET_POLL_READER_COUNT(mac));

        /* CS entery begins */
        SEM_LOCK(&mac_serialize_readers_cs_sem);
                if(is_singly_ll_empty(black_listed_poll_readers_list)){
                         printk(KERN_INFO "%s() poll reader %s\" (pid %i) is the first reader\n",__FUNCTION__, get_current()->comm, get_current()->pid);
                         mac_update_vector_count = mac_get_updated_mac_entries(mac, &mac_update_vector); // this msg is freed by last reader exiting CS
                         printk(KERN_INFO "no of entries to be delivered to each reader = %u, mac_update_vector = %p\n",
                                mac_update_vector_count, mac_update_vector);
                        if(mac_update_vector_count > MAC_MAX_ENTRIES_FETCH)
                                        mac_update_vector_count = MAC_MAX_ENTRIES_FETCH;
                        rc = mac_update_vector_count;
                }
        SEM_UNLOCK(&mac_serialize_readers_cs_sem);
        /* CS entery ends */

        /* CS begins*/
        copy_to_user((void __user *)buf, mac_update_vector, sizeof(struct mac_entry) * mac_update_vector_count);
        rc = mac_update_vector_count;
        /* CS ends*/

        SEM_LOCK(&mac_serialize_readers_cs_sem);

        if(MAC_GET_POLL_READER_COUNT(mac) == 1){
                printk(KERN_INFO "%s() last poll reader \"%s\" (pid %i) has also read the mac update\n", __FUNCTION__, get_current()->comm, get_current()->pid);
                printk(KERN_INFO "%s() last poll reader \"%s\" (pid %i) has deleted the change list\n", __FUNCTION__, get_current()->comm, get_current()->pid);
                mac_empty_change_list(mac);
                rc = mac_update_vector_count;
                mac_update_vector_count = 0;
                kfree(mac_update_vector);
        }

        if(NULL == singly_ll_is_value_present(mac->poll_readers_list, &filp, sizeof(struct filep **))){
                printk(KERN_INFO "%s() Error : poll reader \"%s\" (pid %i) , filep = %p, is not present in mac->poll_readers_list\n", __FUNCTION__, get_current()->comm, get_current()->pid, filp);
                print_singly_LL(mac->poll_readers_list);
        }
        else{
                printk(KERN_INFO "%s() deleting \"%s\" (pid %i) : from mac->poll_readers_list\n", __FUNCTION__, get_current()->comm, get_current()->pid);
                isDeleted = singly_ll_delete_node_by_value(mac->poll_readers_list, (void *)&filp, sizeof(struct filep **));
        }

	printk(KERN_INFO "%s() poll reader \"%s\" (pid %i) has read the data, removed from orig poll readers list, isDeleted = %u\n", __FUNCTION__, get_current()->comm, get_current()->pid, isDeleted);
        singly_ll_add_node_by_val(black_listed_poll_readers_list, (void *)&filp, sizeof(struct filep **));
        printk(KERN_INFO "%s() poll reader \"%s\" (pid %i) has read the data, adding to black lister's list\n", __FUNCTION__, get_current()->comm, get_current()->pid);

        SEM_UNLOCK(&mac_serialize_readers_cs_sem);
        up_read(&mac->rw_sem);
        return rc;
}


ssize_t mac_write (struct file *filp, const char __user *buf, size_t count, loff_t *f_pos){

	struct mac_update_t kmsg;
	printk(KERN_INFO "%s() is called , filep = %p, user buff = %p, buff_size = %ld\n",
			__FUNCTION__,  filp,  buf, count);


	memset(&kmsg, 0, sizeof(struct mac_update_t));

	if(access_ok((void __user*)buf, count) ==  0){
		printk(KERN_INFO "%s() : invalid user space ptr\n", __FUNCTION__);
		return 0;
	}

	copy_from_user(&kmsg, (void __user *)buf, sizeof(struct mac_update_t));

     /* down_write/up_write is same as MAC_LOCK_SEM(mac)/MAC_UNLOCK_SEM(mac) in write case*/
        down_write(&mac->rw_sem); //MAC_LOCK_SEM(mac);
        apply_mac_updates(mac, &kmsg);
        up_write(&mac->rw_sem); //MAC_UNLOCK_SEM(mac);
	printk(KERN_INFO "%s() \"%s\" sending wake_up call to mac->readerQ\n", __FUNCTION__, get_current()->comm);
        wake_up(&mac->readerQ); // wake up the polling reader processes, poll fn is called again

	return sizeof(struct mac_update_t);
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
	
	printk(KERN_INFO "worker thread is locking the writer Queue= %p\n", mac->writer_Q);
	MAC_SEM_LOCK_WRITER_Q(mac);
	
	writer_thread = (struct kernthread *)deque(mac->writer_Q);
	printk(KERN_INFO "worker thread has extracted the writer \"%s\" (pid %i) from writer Queue = %p\n", 
			writer_thread->task->comm, writer_thread->task->pid, mac->writer_Q);

	MAC_SEM_UNLOCK_WRITER_Q(mac);
	printk(KERN_INFO "worker thread has released locked over writer Queue= %p\n", mac->writer_Q);
	
	if(!writer_thread){
		printk(KERN_INFO "worker thread finds there is no writer in writer_Q, worker thread is sleeping\n");
		MAC_SEM_UNLOCK_WRITER_Q(mac);
		printk(KERN_INFO "worker thread has unlocked the writer Queue= %p\n", mac->writer_Q);
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
	printk(KERN_INFO "worker thread finds the no of entries to be delivered to each reader = %u, mac_update_vector = %p\n", 
				mac_update_vector_count, mac_update_vector);
	
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
				if(access_ok((void __user*)arg, sizeof(struct mac_update_t)) ==  0){
					printk(KERN_INFO "%s() : invalid user space ptr\n", __FUNCTION__);
					return rc;
				}
				
				copy_from_user(&kmsg, (void __user *)arg, sizeof(struct mac_update_t));
				printk(KERN_INFO "writer process \"%s\" (pid %i) enters the system\n", 
						writer_thread->task->comm, writer_thread->task->pid);

				printk(KERN_INFO "writer thread \"%s\" (pid %i) is locking the writer Queue = %p, writer Queue count = %u\n", 
						writer_thread->task->comm, writer_thread->task->pid, mac->writer_Q, Q_COUNT(mac->writer_Q));

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
				/* we are locking it because, during polling, the table status could be queried*/
				MAC_LOCK_SEM(mac);
				apply_mac_updates(mac, &kmsg);
				MAC_UNLOCK_SEM(mac);
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

				printk(KERN_INFO "writer thread \"%s\" (pid %i) is locking the writer Queue = %p, writer Queue count = %u\n", 
						writer_thread->task->comm, writer_thread->task->pid, mac->writer_Q, Q_COUNT(mac->writer_Q));

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

				printk(KERN_INFO "writer thread \"%s\" (pid %i) is locking the writer Queue = %p, writer Queue count = %u\n", 
						writer_thread->task->comm, writer_thread->task->pid, mac->writer_Q, Q_COUNT(mac->writer_Q));

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

				if(access_ok((void __user*)arg, sizeof(struct mac_info_t)) ==  0){
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

				counter = GET_NODE_COUNT_SINGLY_LL(black_listed_poll_readers_list);
				put_user(counter, &(mac_info->no_of_blacklisted_polling_readers));

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
			
				if(access_ok((void __user*)arg, sizeof(struct mac_update_t) * MAC_MAX_ENTRIES_FETCH) ==  0){
					printk(KERN_INFO "%s() : invalid user space ptr\n", __FUNCTION__);
					return rc;
				}
			
				printk(KERN_INFO "No of mac updates to be read by reader thread \"%s\" (pid %i) = %u\n", 
					reader_thread->task->comm, reader_thread->task->pid, mac_update_vector_count);	

				if(mac_update_vector_count > MAC_MAX_ENTRIES_FETCH)
					mac_update_vector_count = MAC_MAX_ENTRIES_FETCH;

				printk(KERN_INFO "reader thread \"%s\" (pid %i) is locking the reader Queue = %p, reader Queue count = %u\n",
                                                reader_thread->task->comm, reader_thread->task->pid, mac->reader_Q, Q_COUNT(mac->reader_Q));

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
	printk(KERN_INFO "%s() is called , filep = %p, cmd code = %d\n", __FUNCTION__,  filep, cmd);
	return 0;
}

unsigned int mac_poll (struct file *filep, struct poll_table_struct *poll_table){

#define WAIT_FOR_DATA_AVAILABILITY      0
#define WAIT_FOR_DATA_WRITE             0

	unsigned int isDataAvailable = 0; // 0 not, >0 means yes

	/* if data is not available then choose one out of  two actions. refer rules on page 166, LDD3
	   1. if O_NONBLOCK flag is set, then return immediately with return value -EAGAIN
	   2. if O_NONBLOCK flag is not set, then block untill atleast one byte of data is available
	   return 0;
	 */

	/* lock the mac table, hence, in case if writers writing into mac, mac should be locked by sem*/
	/* Add unique file pointers to the mac->poll_readers_list to keep trackof unique poll readers
	   if file desc is already present, then no op
	 */

	SEM_LOCK(&mac_serialize_readers_cs_sem);
	if (NULL == singly_ll_is_value_present(black_listed_poll_readers_list, &filep, sizeof(struct file **))){
		printk(KERN_INFO "%s() poll reader %s\" (pid %i) is not black listed\n", __FUNCTION__, get_current()->comm, get_current()->pid);
		isDataAvailable = mutex_is_mac_updated(mac); // This operation need not be mutex protected, redundant now
		if(isDataAvailable){
			printk(KERN_INFO "%s() poll reader %s\" (pid %i) finds data is Available, returning POLLIN|POLLRDNORM\n", __FUNCTION__, get_current()->comm, get_current()->pid);
			SEM_UNLOCK(&mac_serialize_readers_cs_sem);
			return POLLIN|POLLRDNORM;
		}
		else{
			/* case 1 : if i am not black listed and data not available*/
			printk(KERN_INFO "%s() poll reader %s\" (pid %i) finds data is not Available, means i am fresh entry into system\n", __FUNCTION__, get_current()->comm, get_current()->pid);
			if(IS_FILE_ACCESS_FLAG_SET(filep, O_NONBLOCK)){
				SEM_UNLOCK(&mac_serialize_readers_cs_sem);
				return -EAGAIN; // select return 2 in user space corresponding to this
			}
			add_mac_table_unique_poll_reader(mac, filep);
			printk(KERN_INFO "%s() poll reader %s\" (pid %i) added to mac orig polling list\n", __FUNCTION__, get_current()->comm, get_current()->pid);
			poll_wait(filep, &mac->readerQ, poll_table);
			printk(KERN_INFO "%s() poll reader %s\" (pid %i) is blocked for Data availablity\n", __FUNCTION__, get_current()->comm, get_current()->pid);
			SEM_UNLOCK(&mac_serialize_readers_cs_sem);
			return WAIT_FOR_DATA_AVAILABILITY;
		}
	}

	else{ /* if i am black listed*/
		printk(KERN_INFO "%s() poll reader %s\" (pid %i) is black listed\n", __FUNCTION__, get_current()->comm, get_current()->pid);
		isDataAvailable = mutex_is_mac_updated(mac);
		if(isDataAvailable){
			/* Black list and data is available */
			printk(KERN_INFO "%s() poll reader %s\" (pid %i) finds data is Available\n", __FUNCTION__, get_current()->comm, get_current()->pid);
			poll_wait(filep, &mac->readerQ, poll_table);
			printk(KERN_INFO "%s() poll reader %s\" (pid %i) has already  read this update, blocking itself\n", __FUNCTION__, get_current()->comm, get_current()->pid);
			SEM_UNLOCK(&mac_serialize_readers_cs_sem);
			return WAIT_FOR_DATA_AVAILABILITY;
		}
		else{ /* Blacklisted but data is not available*/
			printk(KERN_INFO "%s() poll reader %s\" (pid %i) finds data is not Available\n", __FUNCTION__, get_current()->comm, get_current()->pid);
			printk(KERN_INFO "%s() poll reader %s\" (pid %i) is probably the last reader\n", __FUNCTION__, get_current()->comm, get_current()->pid);
			if(MAC_GET_POLL_READER_COUNT(mac) == 0){
				printk(KERN_INFO "%s() mac->poll_readers_list is empty, copying the blacklist list into mac->poll_readers_list\n", __FUNCTION__);

				 /* Do not copy this way, if some new reader comes while old readers have still not yet finished reading the  update,
                                   we may end up in losing the new reader. Copy nodes from black_listed_poll_readers_list to rt->poll_readers_list instead
                                   TBD */

				mac->poll_readers_list->head = black_listed_poll_readers_list->head;
				mac->poll_readers_list->node_count = black_listed_poll_readers_list->node_count;
				black_listed_poll_readers_list->head = NULL;
				black_listed_poll_readers_list->node_count = 0;
			}
			else{
				printk(KERN_INFO "%s() Error : mac->poll_readers_list is not empty !! Count = %d\n", __FUNCTION__, MAC_GET_POLL_READER_COUNT(mac));
			}
			printk(KERN_INFO "%s() black_listed_poll_readers_list count = %d, mac->poll_readers_list_count = %d seen by %s\" (pid %i), and is blocking itself\n",
					__FUNCTION__, black_listed_poll_readers_list->node_count, mac->poll_readers_list->node_count, get_current()->comm, get_current()->pid);
			poll_wait(filep, &mac->readerQ, poll_table);
			SEM_UNLOCK(&mac_serialize_readers_cs_sem);
			return WAIT_FOR_DATA_AVAILABILITY;
		}
	}
}
