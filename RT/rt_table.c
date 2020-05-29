#include "rt_table.h"
#include <linux/slab.h>
#include <linux/uaccess.h>  // for access_ok
#include "rtkernusr.h"
#include "../cmdcodes.h"

extern int rt_worker_fn(void *arg);

/*rt structure to keep track of updated entries writers
to be eventually read by readers */

struct rt_preserve_updates_t{
	unsigned int op_code;
	struct rt_entry *entry;
};


struct rt_table * init_rt_table(void){
	struct rt_table * rt = kmalloc(sizeof(struct rt_table), GFP_KERNEL);
	if(!rt) return NULL; 
	sema_init(&rt->sem, 1);
	init_rwsem(&rt->rw_sem);
	init_completion(&rt->completion);
	spin_lock_init(&rt->spin_lock);
	init_waitqueue_head(&rt->readerQ);
	init_waitqueue_head(&rt->writerQ);
	rt->reader_Q = initQ();
	rt->writer_Q = initQ(); 
	rt->rt = init_singly_ll();
	rt->rt_change_list = init_singly_ll();
	rt->poll_readers_list = init_singly_ll();
	rt->worker_thread = new_kern_thread(&rt_worker_fn, NULL, "rt_worker_thread");
	return rt;
}

int
mutex_rt_get_pollar_readers_count(struct rt_table *rt){
	int count = 0;
	RT_LOCK_SEM(rt);
	count = RT_GET_POLL_READER_COUNT(rt);
	RT_UNLOCK_SEM(rt);
	return count;
}

void
add_rt_table_unique_poll_reader(struct rt_table *rt, struct file *filep){

	struct singly_ll_node_t *node = NULL;
	node = singly_ll_is_value_present(rt->poll_readers_list, &filep, sizeof(struct file **));

	if(node == NULL){
		printk(KERN_INFO "%s() filep =%p added to unique_poll_reader list\n" , __FUNCTION__, filep);
		singly_ll_add_node_by_val(rt->poll_readers_list, &filep, sizeof(struct file **));
		return;
	}
	printk(KERN_INFO "%s() filep =%p already present, ignored \n", __FUNCTION__, filep);
}

int
add_rt_table_entry_by_val(struct rt_table *rt, 
               		 struct rt_entry _rt_entry){

	int rc = SUCCESS;
	struct rt_preserve_updates_t msg;
	memset(&msg, 0, sizeof(struct rt_preserve_updates_t));
	
	printk("%s() is called\n", __FUNCTION__);
	rc = (int)singly_ll_add_node_by_val(rt->rt, &_rt_entry, sizeof(struct rt_entry));
	msg.op_code = RT_ROUTE_ADD;
	msg.entry = (struct rt_entry *)GET_RT_ENTRY_PTR(GET_HEAD_SINGLY_LL(rt->rt));
	singly_ll_add_node_by_val(rt->rt_change_list, &msg, sizeof(struct rt_preserve_updates_t)); 
		
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
	if(access_ok((void __user*)buf, buf_size) ==  0){
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

int 
mutex_is_rt_updated(struct rt_table *rt){
	
	int n = 0;
	if(down_interruptible(&rt->sem))
		return -ERESTARTSYS;

	n = GET_RT_CHANGELIST_ENTRY_COUNT(rt);
	if(!n){
		up(&rt->sem);
		printk(KERN_INFO "Readers detect RT table not yet updated\n");
		return 0;
	}

	printk(KERN_INFO "Readers detect RT table has been updated, new no of entries = %d\n", n);
	up(&rt->sem);
	return 1;
}

int
apply_rt_updates(struct rt_table *rt, struct rt_update_t *update_msg){

	int rc = 0;
	printk("%s() is called\n", __FUNCTION__);
	switch(update_msg->op_code){
		case RT_ROUTE_UPDATE:
			break;
		case RT_ROUTE_ADD:
			add_rt_table_entry_by_val(rt, update_msg->entry);
			break;
		case RT_ROUTE_DEL:
			break;
		case RT_DELETE:
			break;
		default:
			printk(KERN_INFO "%s() Unknown Operation on rt\n", __FUNCTION__);
	}
	return rc;
}

int
rt_empty_change_list(struct rt_table *rt){

	printk("%s() is called\n", __FUNCTION__);
	delete_singly_ll(rt->rt_change_list);
	return 0;
}

int
rt_get_updated_rt_entries(struct rt_table *rt, struct rt_update_t **_rt_update_vector){

	unsigned int count = 0, i = 0;
	struct singly_ll_node_t* head = NULL;
	struct rt_preserve_updates_t *data = NULL;
	struct rt_update_t *rt_update_vector = NULL;

	rt_update_vector = *_rt_update_vector;

	count = GET_RT_CHANGELIST_ENTRY_COUNT(rt);

	rt_update_vector = kzalloc(count * sizeof(struct rt_update_t), GFP_KERNEL);
	 *_rt_update_vector = rt_update_vector;

	if(!rt_update_vector){
		printk(KERN_INFO "%s() memory allocation failed\n", __FUNCTION__);
		return 0;
	}

	head = GET_HEAD_SINGLY_LL(rt->rt_change_list);
	for(;i < count; i++){
		data = GET_RT_ENTRY_PTR(head);
		rt_update_vector->op_code = data->op_code;
		memcpy(&rt_update_vector->entry, data->entry, sizeof(struct rt_entry));
		rt_update_vector++;
		head = GET_NEXT_NODE_SINGLY_LL(head);
	}
	
	return count;
}

