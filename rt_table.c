#include "rt_table.h"
#include "common.h"
#include <linux/slab.h>
#include <linux/uaccess.h>  // for access_ok
#include "kern_usr.h"

struct rt_table * init_rt_table(void){
	struct rt_table * rt = kmalloc(sizeof(struct rt_table), GFP_KERNEL);
	if(!rt) return NULL; 
	sema_init(&rt->sem, 1);
	init_rwsem(&rt->rw_sem);
	init_completion(&rt->completion);
	spin_lock_init(&rt->spin_lock);
	rt->rt = init_singly_ll();
	return rt;
}

int
add_rt_table_entry_by_val(struct rt_table *rt, 
               		 struct rt_entry _rt_entry){

	int rc = SUCCESS;
	rc = (int)singly_ll_add_node_by_val(rt->rt, &_rt_entry, sizeof(struct rt_entry));
	return rc;
}


int
add_rt_table_entry_by_ref(struct rt_table *rt, 
			  struct rt_entry *_rt_entry){

	int rc = SUCCESS;
	struct singly_ll_node_t *node = singly_ll_init_node();
	node->data = _rt_entry;
	node->data_size = sizeof(struct rt_entry);
	rc = (int) singly_ll_add_node (rt->rt, node);
	return rc;
}

struct rt_entry*
lookup_rt_table(struct rt_table *rt, char *ip_key){
	int i = 0;
	struct singly_ll_node_t* head = NULL;
	struct rt_entry *_rt_entry = NULL;

	//down_read(&rt->rw_sem);

	head = GET_HEAD_SINGLY_LL(rt->rt);

	for(; i < GET_NODE_COUNT_SINGLY_LL(rt->rt); i++){
		_rt_entry = (struct rt_entry *)GET_RT_ENTRY_PTR(head);
		if(strncpy(_rt_entry->dst_ip, ip_key, 16) == 0){
			//up_read(&rt->rw_sem);
			return _rt_entry;
		}
		head = GET_RT_NXT_ENTRY_NODE(head);
	}

	//up_read(&rt->rw_sem);
	return NULL;
}


unsigned int
delete_rt_table_entry(struct rt_table *rt, char *ip_key){
	
	unsigned int rc = 0;
	/* returns non zero value when interrupted*/
#if 0
	if(down_interruptible(&rt->sem) != 0)
		return -ERESTARTSYS;
#endif
	rc = singly_ll_delete_node_by_value(rt->rt, ip_key, 16);
	
//	up(&rt->sem);
	return rc;
}

/* return no of entries copied*/
ssize_t
copy_rt_table_to_user_space(struct rt_table *rt, 
			    char __user *buf, 
			    unsigned int buf_size){

	
	struct singly_ll_node_t* head = NULL;
	struct rt_entry *_rt_entry = NULL;
	unsigned int buff_units = 0, max_nodes = 0, i = 0;

	/* Put reader/writer lock*/
//	down_read(&rt->rw_sem);

	 if(IS_RT_TABLE_EMPTY(rt)){
//		up_read(&rt->rw_sem);
                return 0;
	}

	/*validate the user space pointer*/
	if(access_ok(VERIFY_WRITE, (void __user*)buf, buf_size) ==  0){
		 printk(KERN_INFO "%s() : invalid user space ptr\n", __FUNCTION__);
//		 up_read(&rt->rw_sem);
		 return 0;
	}
	
	buff_units = (unsigned int)(buf_size / sizeof(struct rt_entry));
	max_nodes = GET_NODE_COUNT_SINGLY_LL(rt->rt);

	buff_units = (buff_units < max_nodes) ? buff_units : max_nodes;

	head = GET_HEAD_SINGLY_LL(rt->rt);
	_rt_entry = NULL;

	for(; i < buff_units; i++){
		_rt_entry = (struct rt_entry *)GET_RT_ENTRY_PTR(head);
		copy_to_user((void __user *)buf, _rt_entry, sizeof(struct rt_entry));
		buf = (char *)buf + sizeof(struct rt_entry);
		head = GET_RT_NXT_ENTRY_NODE(head);
	}

//	up_read(&rt->rw_sem);

	return (ssize_t)buff_units;
}

void
purge_rt_table(struct rt_table *rt){
	
	/* lock the table*/	
//	down_interruptible(&rt->sem);
	
	delete_singly_ll(rt->rt);
	reinit_completion(&rt->completion);	

	/* unlock the table*/	
//	up(&rt->sem);
}

/* should be clean up using top level mutex*/

void
cleanup_rt_table(struct rt_table **_rt){
	
	struct rt_table *rt = *_rt;
//	down_interruptible(&rt->sem);
	delete_singly_ll(rt->rt);
	kfree(rt->rt);
	kfree(rt);
	rt = NULL;
//	up(&rt->sem);
}
