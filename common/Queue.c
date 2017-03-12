#include "Queue.h"
#include <linux/slab.h>

struct Queue_t* initQ(){
	struct Queue_t *q = kzalloc(sizeof(struct Queue_t), GFP_KERNEL);
	q->rear = Q_DEFAULT_SIZE -1;
	q->front = q->rear;
	sema_init(&q->sem, 1);
        return q;	
}


int
is_queue_empty(struct Queue_t *q){
	if(q->count == 0)
		return 1;
	return 0;
}


int
is_queue_full(struct Queue_t *q){
	if(q->count == Q_DEFAULT_SIZE)
		return 1;
	return 0;
}

int
enqueue(struct Queue_t *q, void *ptr){
	if(!q || !ptr) return 0;
	if(q && is_queue_full(q)){ 
		printk(KERN_INFO "Queue is full\n");
		return 0;
	}
	
	if(is_queue_empty(q)){
		q->elem[q->rear] = ptr;
		printk(KERN_INFO "element inserted at index = %d\n", q->rear);
		q->count++;
		return 1;
	}
		
	if(q->rear == 0){
		if(q->front == Q_DEFAULT_SIZE -1 && is_queue_full(q)){
			printk(KERN_INFO "Queue is full\n");
			return 0;
		}
		q->rear = Q_DEFAULT_SIZE -1;
		q->elem[q->rear] = ptr;
		q->count++;
		printk(KERN_INFO "element inserted at index = %d\n", q->rear);
		return 1;
	}

	q->rear--;
	q->elem[q->rear] = ptr;
	q->count++;
	printk(KERN_INFO "element inserted at index = %d\n", q->rear);
	return 1;
}

void*
deque(struct Queue_t *q){
	
	void *elem = NULL;
	if(!q) return NULL;
	if(is_queue_empty(q))
		return NULL;

	elem = q->elem[q->front];
	q->elem[q->front] = NULL;
	// for last elem
	if(q->front == q->rear){
		q->count--;
		return elem;
	}

	if(q->front == 0)
		q->front = Q_DEFAULT_SIZE -1;
	else
		q->front--;
	q->count--;
	return elem;
}

void
print_Queue(struct Queue_t *q){
	unsigned int i = 0;
	printk(KERN_INFO "q->front = %d, q->rear = %d, q->count = %d\n", q->front, q->rear, q->count);
	for(i = 0; i < Q_DEFAULT_SIZE; i++){
		if(q->elem[i] == NULL)
			continue;
		printk(KERN_INFO "index = %u, elem = 0x%x\n", i, (unsigned int)q->elem[i]);
	}
}

#if 0
int 
main(int argc, char **argv){
return 0;	
}
#endif
