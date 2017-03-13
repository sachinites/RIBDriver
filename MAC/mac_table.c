#include "mac_table.h"
#include <linux/slab.h>
#include <linux/uaccess.h>  // for access_ok
#include "mackernusr.h"
#include "../cmdcodes.h"

extern int mac_worker_fn(void *arg);

/*mac structure to keep track of updated entries writers
to be eventually read by readers */

struct mac_preserve_updates_t{
	unsigned int op_code;
	struct mac_entry *entry;
};


struct mac_table * init_mac_table(void){
	struct mac_table * mac = kmalloc(sizeof(struct mac_table), GFP_KERNEL);
	if(!mac) return NULL; 
	sema_init(&mac->sem, 1);
	init_rwsem(&mac->rw_sem);
	init_completion(&mac->completion);
	spin_lock_init(&mac->spin_lock);
	init_waitqueue_head(&mac->readerQ);
	init_waitqueue_head(&mac->writerQ);
	mac->reader_Q = initQ();
	mac->writer_Q = initQ(); 
	mac->mac = init_singly_ll();
	mac->mac_change_list = init_singly_ll();
	mac->poll_readers_list = init_singly_ll();
	mac->worker_thread = new_kern_thread(&mac_worker_fn, NULL, "mac_worker_thread");
	return mac;
}

int
mutex_mac_get_pollar_readers_count(struct mac_table *mac){
        int count = 0;
        MAC_LOCK_SEM(mac);
        count = MAC_GET_POLL_READER_COUNT(mac);
        MAC_UNLOCK_SEM(mac);
        return count;
}

void
add_mac_table_unique_poll_reader(struct mac_table *mac, struct file *filep){

        struct singly_ll_node_t *node = NULL;
        node = singly_ll_is_value_present(mac->poll_readers_list, &filep, sizeof(struct file **));

        if(node == NULL){
                printk(KERN_INFO "%s() filep =0x%x added to unique_poll_reader list\n" , __FUNCTION__, (unsigned int)filep);
                singly_ll_add_node_by_val(mac->poll_readers_list, &filep, sizeof(struct file **));
                return;
        }
        printk(KERN_INFO "%s() filep =0x%x already present, ignored \n", __FUNCTION__, (unsigned int)filep);
}



int
add_mac_table_entry_by_val(struct mac_table *mac, 
               		 struct mac_entry _mac_entry){

	int rc = SUCCESS;
	struct mac_preserve_updates_t msg;
	memset(&msg, 0, sizeof(struct mac_preserve_updates_t));
	
	printk("%s() is called\n", __FUNCTION__);
	rc = (int)singly_ll_add_node_by_val(mac->mac, &_mac_entry, sizeof(struct mac_entry));
	msg.op_code = MAC_ROUTE_ADD;
	msg.entry = (struct mac_entry *)GET_MAC_ENTRY_PTR(GET_HEAD_SINGLY_LL(mac->mac));
	singly_ll_add_node_by_val(mac->mac_change_list, &msg, sizeof(struct mac_preserve_updates_t)); 
		
	return rc;
}


int
add_mac_table_entry_by_ref(struct mac_table *mac, 
			  struct mac_entry *_mac_entry){

	int rc = SUCCESS;
	struct singly_ll_node_t *node = singly_ll_init_node();
	node->data = _mac_entry;
	node->data_size = sizeof(struct mac_entry);
	rc = (int) singly_ll_add_node (mac->mac, node);
	return rc;
}

struct mac_entry*
lookup_mac_table(struct mac_table *mac, char *ip_key){
	int i = 0;
	struct singly_ll_node_t* head = NULL;
	struct mac_entry *_mac_entry = NULL;

	//down_read(&mac->rw_sem);

	head = GET_HEAD_SINGLY_LL(mac->mac);

	for(; i < GET_NODE_COUNT_SINGLY_LL(mac->mac); i++){
		_mac_entry = (struct mac_entry *)GET_MAC_ENTRY_PTR(head);
		if(strncpy(_mac_entry->vlan_id, ip_key, 16) == 0){
			//up_read(&mac->rw_sem);
			return _mac_entry;
		}
		head = GET_MAC_NXT_ENTRY_NODE(head);
	}

	//up_read(&mac->rw_sem);
	return NULL;
}


unsigned int
delete_mac_table_entry(struct mac_table *mac, char *ip_key){
	
	unsigned int rc = 0;
	/* returns non zero value when interrupted*/
#if 0
	if(down_interruptible(&mac->sem) != 0)
		return -ERESTAMACSYS;
#endif
	rc = singly_ll_delete_node_by_value(mac->mac, ip_key, 16);
	
//	up(&mac->sem);
	return rc;
}

/* return no of entries copied*/
ssize_t
copy_mac_table_to_user_space(struct mac_table *mac, 
			    char __user *buf, 
			    unsigned int buf_size){

	
	struct singly_ll_node_t* head = NULL;
	struct mac_entry *_mac_entry = NULL;
	unsigned int buff_units = 0, max_nodes = 0, i = 0;

	/* Put reader/writer lock*/
//	down_read(&mac->rw_sem);

	 if(IS_MAC_TABLE_EMPTY(mac)){
//		up_read(&mac->rw_sem);
                return 0;
	}

	/*validate the user space pointer*/
	if(access_ok(VERIFY_WRITE, (void __user*)buf, buf_size) ==  0){
		 printk(KERN_INFO "%s() : invalid user space ptr\n", __FUNCTION__);
//		 up_read(&mac->rw_sem);
		 return 0;
	}
	
	buff_units = (unsigned int)(buf_size / sizeof(struct mac_entry));
	max_nodes = GET_NODE_COUNT_SINGLY_LL(mac->mac);

	buff_units = (buff_units < max_nodes) ? buff_units : max_nodes;

	head = GET_HEAD_SINGLY_LL(mac->mac);
	_mac_entry = NULL;

	for(; i < buff_units; i++){
		_mac_entry = (struct mac_entry *)GET_MAC_ENTRY_PTR(head);
		copy_to_user((void __user *)buf, _mac_entry, sizeof(struct mac_entry));
		buf = (char *)buf + sizeof(struct mac_entry);
		head = GET_MAC_NXT_ENTRY_NODE(head);
	}

//	up_read(&mac->rw_sem);

	return (ssize_t)buff_units;
}

void
purge_mac_table(struct mac_table *mac){
	
	/* lock the table*/	
//	down_interruptible(&mac->sem);
	
	delete_singly_ll(mac->mac);
	reinit_completion(&mac->completion);	

	/* unlock the table*/	
//	up(&mac->sem);
}

/* should be clean up using top level mutex*/

void
cleanup_mac_table(struct mac_table **_mac){
	
	struct mac_table *mac = *_mac;
//	down_interruptible(&mac->sem);
	delete_singly_ll(mac->mac);
	kfree(mac->mac);
	kfree(mac);
	mac = NULL;
//	up(&mac->sem);
}

int 
mutex_is_mac_updated(struct mac_table *mac){
        
        int n = 0;
        if(down_interruptible(&mac->sem))
                return -ERESTARTSYS;

        n = GET_MAC_CHANGELIST_ENTRY_COUNT(mac);
        if(!n){
                up(&mac->sem);
                printk(KERN_INFO "Readers detect MAC table not yet updated\n");
                return 0;
        }

        printk(KERN_INFO "Readers detect MAC table has been updated, new no of entries = %d\n", n);
        up(&mac->sem);
        return 1;	
}

int
apply_mac_updates(struct mac_table *mac, struct mac_update_t *update_msg){

	int rc = 0;
	printk("%s() is called\n", __FUNCTION__);
	switch(update_msg->op_code){
		case MAC_ROUTE_UPDATE:
			break;
		case MAC_ROUTE_ADD:
			add_mac_table_entry_by_val(mac, update_msg->entry);
			break;
		case MAC_ROUTE_DEL:
			break;
		case MAC_DELETE:
			break;
		default:
			printk(KERN_INFO "%s() Unknown Operation on mac\n", __FUNCTION__);
	}
	return rc;
}

int
mac_empty_change_list(struct mac_table *mac){

	printk("%s() is called\n", __FUNCTION__);
	delete_singly_ll(mac->mac_change_list);
	return 0;
}

int
mac_get_updated_mac_entries(struct mac_table *mac, struct mac_update_t **_mac_update_vector){

	unsigned int count = 0, i = 0;
	struct singly_ll_node_t* head = NULL;
	struct mac_preserve_updates_t *data = NULL;
	struct mac_update_t *mac_update_vector = NULL;

	mac_update_vector = *_mac_update_vector;

	count = GET_MAC_CHANGELIST_ENTRY_COUNT(mac);

	mac_update_vector = kzalloc(count * sizeof(struct mac_update_t), GFP_KERNEL);
	 *_mac_update_vector = mac_update_vector;

	if(!mac_update_vector){
		printk(KERN_INFO "%s() memory allocation failed\n", __FUNCTION__);
		return 0;
	}

	head = GET_HEAD_SINGLY_LL(mac->mac_change_list);
	for(;i < count; i++){
		data = GET_MAC_ENTRY_PTR(head);
		mac_update_vector->op_code = data->op_code;
		memcpy(&mac_update_vector->entry, data->entry, sizeof(struct mac_entry));
		mac_update_vector++;
		head = GET_NEXT_NODE_SINGLY_LL(head);
	}
	
	return count;
}

