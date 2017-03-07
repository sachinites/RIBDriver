#ifndef __RT_TABLE_H__
#define __RT_TABLE_H__

#include <linux/semaphore.h>
#include <linux/rwsem.h>
#include <linux/completion.h>
#include <linux/spinlock.h>
#include <linux/cdev.h>
#include "LinkedListApi.h"

/*Shared structure defined in kern_usr.h*/
struct rt_entry ;

struct rt_table{
	struct ll_t *rt;
	struct semaphore sem;
	struct rw_semaphore rw_sem;
	struct completion completion;
	spinlock_t spin_lock;	
	struct cdev cdev;
};

#define IS_RT_TABLE_EMPTY(rt)		((unsigned int)is_singly_ll_empty(rt->rt))
#define GET_RT_NXT_ENTRY_NODE(node)	(GET_NEXT_NODE_SINGLY_LL(((struct singly_ll_node_t *)node)))
#define GET_RT_ENTRY_PTR(node)		(((struct singly_ll_node_t *)node)->data)
#define GET_RT_ENTRY_COUNT(rt)		(GET_NODE_COUNT_SINGLY_LL(rt->rt))
#define GET_FIRST_RT_ENTRY_NODE(rt)	((void *)GET_HEAD_SINGLY_LL(rt->rt))

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

#endif
