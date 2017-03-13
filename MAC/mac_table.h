#ifndef __MAC_TABLE_H__
#define __MAC_TABLE_H__

#include <linux/semaphore.h>
#include <linux/rwsem.h>
#include <linux/completion.h>
#include <linux/spinlock.h>
#include <linux/cdev.h>
#include "../common/LinkedListApi.h"
#include <linux/wait.h>  // for waitQueues
#include <linux/sched.h>
#include  "../common/kernthread.h"
#include "../common/Queue.h"
#include "../common/kernutils.h"

/*Shared structure defined in kern_usr.h*/
struct mac_entry ;
struct mac_update_t;

struct mac_table{
	struct ll_t *mac;
	struct semaphore sem;
	struct rw_semaphore rw_sem;
	struct completion completion;
	spinlock_t spin_lock;
	wait_queue_head_t readerQ; // Queue of processes waiting to read the MAC update
	wait_queue_head_t writerQ; // Queue of processes waiting to write to MAC	
	struct kernthread *worker_thread; // every resouce has a worker thread whose job is to share the resource between readers writers
	struct Queue_t *reader_Q;
	struct Queue_t *writer_Q;	
	struct ll_t *mac_change_list;	
	struct ll_t *poll_readers_list;
	struct cdev cdev;
};

#define IS_MAC_TABLE_EMPTY(mac)		((unsigned int)is_singly_ll_empty(mac->mac))
#define GET_MAC_NXT_ENTRY_NODE(node)	(GET_NEXT_NODE_SINGLY_LL(((struct singly_ll_node_t *)node)))
#define GET_MAC_ENTRY_PTR(node)		(((struct singly_ll_node_t *)node)->data)
#define GET_MAC_ENTRY_COUNT(mac)		(GET_NODE_COUNT_SINGLY_LL(mac->mac))
#define GET_FIRST_MAC_ENTRY_NODE(mac)	((void *)GET_HEAD_SINGLY_LL(mac->mac))
#define GET_MAC_CHANGELIST_ENTRY_COUNT(mac)	(GET_NODE_COUNT_SINGLY_LL(mac->mac_change_list))


#define  MAC_SEM_LOCK_WRITER_Q(mac)       (SEM_LOCK(&mac->writer_Q->sem))
#define  MAC_SEM_UNLOCK_WRITER_Q(mac)     (SEM_UNLOCK(&mac->writer_Q->sem))

#define  MAC_SEM_LOCK_READER_Q(mac)       (SEM_LOCK(&mac->reader_Q->sem))
#define  MAC_SEM_UNLOCK_READER_Q(mac)     (SEM_UNLOCK(&mac->reader_Q->sem))

#define MAC_LOCK_SEM(mac)                 (SEM_LOCK(&mac->sem))
#define MAC_UNLOCK_SEM(mac)               (SEM_UNLOCK(&mac->sem))

#define MAC_GET_POLL_READER_COUNT(mac)   (GET_NODE_COUNT_SINGLY_LL(mac->poll_readers_list))
#define MAC_REMOVE_POLL_READER(mac, ptr) (singly_ll_remove_node_by_value(mac->poll_readers_list, ptr, sizeof(void *)))


struct mac_table * init_mac_table(void);

/*return 0 on success*/
int
add_mac_table_entry_by_val(struct mac_table *mac, struct mac_entry _mac_entry);

/*return 0 on success*/
int
add_mac_table_entry_by_ref(struct mac_table *mac, struct mac_entry *_mac_entry);

struct mac_entry*
lookup_mac_table(struct mac_table *mac, char *ip_key);

unsigned int
delete_mac_table_entry(struct mac_table *mac, char *ip_key);

ssize_t
copy_mac_table_to_user_space(struct mac_table *mac, char __user *buf, unsigned int buf_size);


void purge_mac_table(struct mac_table *mac);

void cleanup_mac_table(struct mac_table **mac);

int
mutex_is_mac_updated(struct mac_table *mac);

void mac_set_worker_thread_fn(struct mac_table *mac, void *fn, void *fn_arg);

int
apply_mac_updates(struct mac_table *mac, struct mac_update_t *update_msg);

int
mac_empty_change_list(struct mac_table *mac);

int
mac_get_updated_mac_entries(struct mac_table *mac, struct mac_update_t **mac_update_vector);

struct file;

void
add_mac_table_unique_poll_reader(struct mac_table *mac, struct file *filep);


#endif
