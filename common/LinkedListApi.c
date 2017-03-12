#include "LinkedListApi.h"
#include <linux/slab.h>         /* kmalloc() */
#include <linux/kernel.h>       /* printk() */
#include "kernutils.h"

struct ll_t* init_singly_ll(void){
    struct ll_t* list = kmalloc(sizeof(struct ll_t), GFP_KERNEL);
    memset(list, 0 , sizeof(struct ll_t));
    return list;
}

struct singly_ll_node_t* singly_ll_init_node(void){
    struct singly_ll_node_t* node = kmalloc(sizeof(struct singly_ll_node_t), GFP_KERNEL);
    memset(node, 0, sizeof(struct singly_ll_node_t));
    return node;
}

enum rc_t
singly_ll_delete_node(struct ll_t *ll, struct singly_ll_node_t *node){

    struct singly_ll_node_t *trav = NULL, *temp = NULL;

    if(!ll) return LL_FAILURE;
    if(!GET_HEAD_SINGLY_LL(ll) || !node) return 0;
    /*if node is not the last node*/
    if(node->next){
        node->data = node->next->data;
        temp = node->next;
        node->next = node->next->next;
	if(temp->data) kfree(temp->data);
        kfree(temp);
        DEC_NODE_COUNT_SINGLY_LL(ll);
        return LL_SUCCESS;
    }

    /* if node is the only node in LL*/
    if(ll->node_count == 1 && GET_HEAD_SINGLY_LL(ll) == node){
	if(node->data) kfree(node->data);
        kfree(node);
        GET_HEAD_SINGLY_LL(ll) = NULL;
        DEC_NODE_COUNT_SINGLY_LL(ll);
        return LL_SUCCESS;
    }

    /*if node is the last node of the LL*/
    trav = GET_HEAD_SINGLY_LL(ll);
    while(trav->next != node){
        trav = trav->next;
        continue;
    }

    trav->next = NULL;
    if(node->data) kfree(node->data);
    kfree(node);
    DEC_NODE_COUNT_SINGLY_LL(ll);
    return LL_SUCCESS;
}

unsigned int
singly_ll_delete_node_by_value(struct ll_t *ll, void *key, unsigned int size){
    
    unsigned int curren_node_count = GET_NODE_COUNT_SINGLY_LL(ll);
    struct singly_ll_node_t* trav = GET_HEAD_SINGLY_LL(ll);

    if(!ll || !GET_HEAD_SINGLY_LL(ll)) return 0;
    while(trav != NULL){
        if(memcmp(trav->data, key, size) == 0){
            singly_ll_delete_node(ll, trav);
            return curren_node_count - GET_NODE_COUNT_SINGLY_LL(ll);
        }
        trav = trav->next;
    }
    return curren_node_count - GET_NODE_COUNT_SINGLY_LL(ll);
}

struct singly_ll_node_t*
singly_ll_get_node_by_data_ptr(struct ll_t *ll, void *data){
        
	int i = 0;
        struct singly_ll_node_t *head = GET_HEAD_SINGLY_LL(ll);

        if(!ll || !GET_HEAD_SINGLY_LL(ll)) return NULL;
        for(; i < GET_NODE_COUNT_SINGLY_LL(ll); i++){
                if(head->data == data)
                        return head;
                head = GET_NEXT_NODE_SINGLY_LL(head);
        }
        return NULL;
}

enum rc_t 
singly_ll_add_node(struct ll_t* ll, struct singly_ll_node_t *node){
    if(!ll) return LL_FAILURE;
    if(!node) return LL_FAILURE;
    if(!GET_HEAD_SINGLY_LL(ll)){
        GET_HEAD_SINGLY_LL(ll) = node;
        INC_NODE_COUNT_SINGLY_LL(ll);
        return LL_SUCCESS;
    }

    node->next = GET_HEAD_SINGLY_LL(ll);
    GET_HEAD_SINGLY_LL(ll) = node;
    INC_NODE_COUNT_SINGLY_LL(ll);
    return LL_SUCCESS;
}

enum rc_t 
singly_ll_add_node_by_val(struct ll_t *ll, void *data, unsigned int data_size){

    struct singly_ll_node_t* node = singly_ll_init_node();
    node->data = kmalloc(data_size, GFP_KERNEL);
    node->data_size = data_size;
    memcpy(node->data, data, data_size);
    return singly_ll_add_node(ll, node);
}

enum rc_t
singly_ll_remove_node(struct ll_t *ll, struct singly_ll_node_t *node){

    struct singly_ll_node_t *trav = NULL;

    if(!ll) return LL_FAILURE;
    if(!GET_HEAD_SINGLY_LL(ll) || !node) return LL_SUCCESS;
    /*if node is not the last node*/
    if(node->next){
        struct singly_ll_node_t *temp = NULL;
        node->data = node->next->data;
        temp = node->next;
        node->next = node->next->next;
	if(temp->data) kfree(temp->data);
        kfree(temp);
        DEC_NODE_COUNT_SINGLY_LL(ll);
        return LL_SUCCESS;
    }

    /* if node is the only node in LL*/
    if(ll->node_count == 1 && GET_HEAD_SINGLY_LL(ll) == node){
	if(node->data) kfree(node->data);
        kfree(node);
        GET_HEAD_SINGLY_LL(ll) = NULL;
        DEC_NODE_COUNT_SINGLY_LL(ll);
        return LL_SUCCESS;
    }

    /*if node is the last node of the LL*/
    trav = GET_HEAD_SINGLY_LL(ll);
    while(trav->next != node){
        trav = trav->next;
        continue;
    }
    
    trav->next = NULL;
    if(node->data) kfree(node->data);
    kfree(node);
    DEC_NODE_COUNT_SINGLY_LL(ll);
    return LL_SUCCESS;
}

unsigned int
singly_ll_remove_node_by_value(struct ll_t *ll, void *key, unsigned int key_size){
    
    unsigned int curren_node_count = 0; 
    struct singly_ll_node_t* trav = NULL;

    if(!ll || !GET_HEAD_SINGLY_LL(ll)) return 0;

    curren_node_count = GET_NODE_COUNT_SINGLY_LL(ll);
    trav = GET_HEAD_SINGLY_LL(ll);

    while(trav != NULL){
        if(memcmp(trav->data, key, key_size) == 0){
            singly_ll_remove_node(ll, trav);
	    return curren_node_count - GET_NODE_COUNT_SINGLY_LL(ll);
        }
        trav = trav->next;
    }
    return curren_node_count - GET_NODE_COUNT_SINGLY_LL(ll);
}

void print_singly_LL(struct ll_t *ll){

    struct singly_ll_node_t* trav = NULL;
    unsigned int i = 0;

    if(!ll) {
        printk(KERN_INFO "Invalid Linked List\n"); 
        return;
    }
    if(is_singly_ll_empty(ll)){
        printk(KERN_INFO "Empty Linked List\n");
        return;
    }
    
    trav = GET_HEAD_SINGLY_LL(ll);
    i = 0;
    printk(KERN_INFO "node count = %d\n", GET_NODE_COUNT_SINGLY_LL(ll));
    while(trav){
        printk(KERN_INFO "%d. Data = 0x%x, data_size = %d\n", i, (unsigned int)trav->data, trav->data_size);
        i++;
        trav = trav->next;
    }
}

enum bool_t 
is_singly_ll_empty(struct ll_t *ll){
    if(!ll) assert(0);
    if(ll->node_count == 0)
        return TRUE;
    return FALSE;
}

void 
reverse_singly_ll(struct ll_t *ll){

   struct singly_ll_node_t *p1 = NULL, *p2 = NULL, 
			   *p3 = NULL;

   if(!ll) assert(0) ;
   if(is_singly_ll_empty(ll)) return;
   if(GET_NODE_COUNT_SINGLY_LL(ll) == 1) return;
   p1 = GET_HEAD_SINGLY_LL(ll), p2 = ll->head->next, 
			      p3 = NULL;
   p1->next = NULL;
   do{
        p3 = p2->next;
        p2->next = p1;
        p1 = p2;
        p2 = p3;
   }while(p3);
   ll->head = p1;
   return;
}

void _reverse_singly_ll2(struct singly_ll_node_t *head){

	if(head->next->next)
		_reverse_singly_ll2(head->next);

	head->next->next = head;	
}



void
reverse_singly_ll2(struct ll_t *ll){

	struct singly_ll_node_t *last_node = NULL;

	if(!ll) assert(0) ;
	if(is_singly_ll_empty(ll)) return;

	if(GET_NODE_COUNT_SINGLY_LL(ll) == 1) return;

	last_node = GET_HEAD_SINGLY_LL(ll);

	while(last_node->next)
		last_node = last_node->next;

	_reverse_singly_ll2(GET_HEAD_SINGLY_LL(ll));

	(GET_HEAD_SINGLY_LL(ll))->next = NULL;

	GET_HEAD_SINGLY_LL(ll) = last_node;
}


void
_reverse_singly_ll3(struct singly_ll_node_t *prev, struct singly_ll_node_t *next){

	if(next->next)
		_reverse_singly_ll3(next, next->next);
	
	next->next = prev;
}

void
reverse_singly_ll3(struct ll_t *ll){

	struct singly_ll_node_t *last_node = NULL;
	if(!ll) assert(0) ;
	if(is_singly_ll_empty(ll)) return;

	if(GET_NODE_COUNT_SINGLY_LL(ll) == 1) return;
	
	last_node = GET_HEAD_SINGLY_LL(ll);

	while(last_node->next)
		last_node = last_node->next;

	_reverse_singly_ll3(NULL, GET_HEAD_SINGLY_LL(ll));

	GET_HEAD_SINGLY_LL(ll) = last_node;
}

void
delete_singly_ll(struct ll_t *ll){

	struct singly_ll_node_t *head = NULL, 
				*next = NULL;
        if(!ll) return;

        if(is_singly_ll_empty(ll)){
                return;
        }

        head = GET_HEAD_SINGLY_LL(ll);
        next = GET_NEXT_NODE_SINGLY_LL(head);

        do{
		if(head->data) kfree(head->data);
                kfree(head);
                head = next;
                if(next)
                        next = GET_NEXT_NODE_SINGLY_LL(next);

        } while(head);

        ll->node_count = 0;
        ll->head = NULL;
}

