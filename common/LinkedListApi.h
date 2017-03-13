#ifndef __LL_API_H__
#define __LL_API_H__

#define GET_HEAD_SINGLY_LL(ll) (ll->head)
#define INC_NODE_COUNT_SINGLY_LL(ll) (ll->node_count++)
#define DEC_NODE_COUNT_SINGLY_LL(ll) (ll->node_count--)
#define GET_NODE_COUNT_SINGLY_LL(ll) (ll->node_count)
#define GET_NEXT_NODE_SINGLY_LL(node) (node->next)

enum rc_t{
    LL_SUCCESS,
    LL_FAILURE
};

enum bool_t{
    FALSE,
    TRUE
};

struct singly_ll_node_t{
    void *data;
    unsigned int data_size;
    struct singly_ll_node_t *next;
};

struct ll_t{
    unsigned int node_count;
    struct singly_ll_node_t *head;
};

struct ll_t* init_singly_ll(void);
struct singly_ll_node_t* singly_ll_init_node(void);
enum rc_t singly_ll_add_node(struct ll_t *ll, struct singly_ll_node_t *node);
enum rc_t singly_ll_add_node_by_val(struct ll_t *ll, void *data, unsigned int data_size);
enum rc_t singly_ll_remove_node(struct ll_t *ll, struct singly_ll_node_t *node);
unsigned int singly_ll_remove_node_by_value(struct ll_t *ll, void *key, unsigned int key_size);
enum bool_t is_singly_ll_empty(struct ll_t *ll);
void print_singly_LL(struct ll_t *ll);
void reverse_singly_ll(struct ll_t *ll);
void reverse_singly_ll2(struct ll_t *ll);
void reverse_singly_ll3(struct ll_t *ll);
/*delete APIs*/
void delete_singly_ll(struct ll_t *ll);
enum rc_t singly_ll_delete_node(struct ll_t *ll, struct singly_ll_node_t *node);
struct singly_ll_node_t *singly_ll_get_node_by_data_ptr(struct ll_t *ll, void *data);
unsigned int singly_ll_delete_node_by_value(struct ll_t *ll, void *key, unsigned int key_size);
struct singly_ll_node_t* singly_ll_is_value_present(struct ll_t *ll, void *data, unsigned int data_size);
#endif
