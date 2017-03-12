#ifndef __RT_TABLE_H__
#define __RT_TABLE_H__

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
struct rt_entry ;
struct rt_update_t;

struct rt_table{
	struct ll_t *rt;
	struct semaphore sem;
	struct rw_semaphore rw_sem;
	struct completion completion;
	spinlock_t spin_lock;
	wait_queue_head_t readerQ; // Queue of processes waiting to read the RT update
	wait_queue_head_t writerQ; // Queue of processes waiting to write to RT	
	struct kernthread *worker_thread; // every resouce has a worker thread whose job is to share the resource between readers writers
	struct Queue_t *reader_Q;
	struct Queue_t *writer_Q;	
	struct ll_t *rt_change_list;	
	struct cdev cdev;
};

#define IS_RT_TABLE_EMPTY(rt)		((unsigned int)is_singly_ll_empty(rt->rt))
#define GET_RT_NXT_ENTRY_NODE(node)	(GET_NEXT_NODE_SINGLY_LL(((struct singly_ll_node_t *)node)))
#define GET_RT_ENTRY_PTR(node)		(((struct singly_ll_node_t *)node)->data)
#define GET_RT_ENTRY_COUNT(rt)		(GET_NODE_COUNT_SINGLY_LL(rt->rt))
#define GET_FIRST_RT_ENTRY_NODE(rt)	((void *)GET_HEAD_SINGLY_LL(rt->rt))
#define GET_RT_CHANGELIST_ENTRY_COUNT(rt)	(GET_NODE_COUNT_SINGLY_LL(rt->rt_change_list))

#define  RT_SEM_LOCK_WRITER_Q(rt)	(SEM_LOCK(&rt->writer_Q->sem))
#define  RT_SEM_UNLOCK_WRITER_Q(rt)	(SEM_UNLOCK(&rt->writer_Q->sem))

#define  RT_SEM_LOCK_READER_Q(rt)	(SEM_LOCK(&rt->reader_Q->sem)) 
#define  RT_SEM_UNLOCK_READER_Q(rt)	(SEM_UNLOCK(&rt->reader_Q->sem))


struct rt_table * init_rt_table(void);

/*return 0 on success*/
int
add_rt_table_entry_by_val(struct rt_table *rt, struct rt_entry _rt_entry);

/*return 0 on success*/
int
add_rt_table_entry_by_ref(struct rt_table *rt, struct rt_entry *_rt_entry);

struct rt_entry*
lookup_rt_table(struct rt_table *rt, char *ip_key);

unsigned int
delete_rt_table_entry(struct rt_table *rt, char *ip_key);

ssize_t
copy_rt_table_to_user_space(struct rt_table *rt, char __user *buf, unsigned int buf_size);


void purge_rt_table(struct rt_table *rt);

void cleanup_rt_table(struct rt_table **rt);

int
mutex_is_rt_updated(unsigned int intial_node_cnt, struct rt_table *rt);

void rt_set_worker_thread_fn(struct rt_table *rt, void *fn, void *fn_arg);

int
apply_rt_updates(struct rt_table *rt, struct rt_update_t *update_msg);

int
rt_empty_change_list(struct rt_table *rt);

int
rt_get_updated_rt_entries(struct rt_table *rt, struct rt_update_t **rt_update_vector);

#endif
