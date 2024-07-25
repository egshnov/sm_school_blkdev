#include "memtable.h"
#include "hash.h"
#include <linux/slab.h>
#include <linux/string.h>

/*fixed size array of bytes for storing keys and values*/
struct byte_array {
	uint8_t *data;
	size_t data_size;
};

struct *byte_array byte_array_create(uint8_t *data, size_t data_size)
{
	struct byte_array *res = kzalloc(sizeof(*res), GFP_KERNEL);
	if (!res)
		return NULL;

	res->data_size = data_size;
	res->data = kzalloc(sizeof(uint8_t) * data_size, GFP_KERNEL);

	if (!res->data)
		goto dealloc;

	memcpy(res->data, data, data_size);
	return res;

dealloc:
	kfree(res);
	return NULL;
}

void byte_array_free(struct byte_array *arr)
{
	if (arr) {
		kfree(arr->data);
		kfree(arr);
	}
}

/* -1 lhs < rhs
    1 lhs > rhs
    0 lhs == rhs */
static int __byte_array_compare(uint8_t *lhs_data, size_t lhs_data_size,
				uint8_t *rhs_data, size_t rhs_data_size)
{
	if (lhs_data_size != rhs_data_size)
		return lhs_data_size < rhs_data_size ? -1 : 1;

	for (int i = 0; i < lhs_data_size; i++) {
		if (lhs_data[i] != rhs_data[i])
			return lhs_data[i] < rhs_data[i] ? -1 : 1;
	}
	return 0;
};

int byte_array_compare(struct byte_array *lhs, struct byte_array *rhs)
{
	return __byte_array_compare(lhs->data, lhs->data_size, rhs->data,
				    rhs->data_size);
}

/*kv-pair for stroing inside of rb_tree*/
struct byte_array_pair {
	byte_array *key;
	byte_array *value;
};

static void bap_free(struct byte_array_pair *bap)
{
	if (bap) {
		byte_array_free(bap->key);
		byte_array_free(bap->value);
		kfree(bap);
	}
}

static struct byte_array_pair *bap_create(uint8_t *key, size_t key_size,
					  uint8_t *value, size_t value_size)
{
	struct byte_array_pair *bap = kzalloc(sizeof(*bap), GFP_KERNEL);

	if (!bap)
		return NULL;

	bap->key = byte_array_create(key, key_size);
	bap->value = byte_array_create(data, data_size);
	if (!bap->key || !bap->value)
		goto dealloc;

	return bap;

dealloc:
	bap_free(bap);
	return NULL;
}

static uint32_t bap_hash(struct byte_array_pair *bap)
{
	return murmurhash(bap->key, bap->key_size, 0);
}

static int bap_compare(struct byte_array_pair *lhs, struct byte_array_pair *rhs)
{
	return byte_array_compare(lhs->key, rhs->key);
}

static size_t bap_data_size(struct byte_array_pair *bap)
{
	return bap->key->data_size + bap->value->data_size;
}

static char *bap_to_str(struct byte_array_pair *bap)
{
	char *res =
		kzalloc(sizeof(char) * (bap_data_size(bap) + 1), GFP_KERNEL);

	if (!res)
		return NULL;

	int i;
	for (i = 0; i < bap->key->data_size; i++) {
		res[i] = bap->key->data[i];
	}

	res[i] = ':';

	for (; i - bap->key->data_size < bap->value->data_size; i++) {
		res[i] = bap->value->data[i - bap->key->data_size];
	}
	res[i] = '\0';
	return res;
}

//TODO: ADD CONDITIONAL COMPILATION FOR SKIP_LIST WHEN READY
/*struct for wrapping kv - pairs to use with kernels rb_tree*/
struct mtb_node {
	struct rb_node node;
	struct byte_array_pair *bap;
}

static struct mtb_node *
mtb_node_create(uint8_t *key, size_t key_size, uint8_t *value,
		size_t value_size)
{
	struct mtb_node *node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node)
		return NULL;

	node->bap = bap_create(key, key_size, value, value_size);
	if (!node->bap)
		goto dealloc;

	return node;
dealloc:
	kfree(node);
	return NULL:
}

static void mtb_node_free(struct mtb_node *node)
{
	if (node) {
		bap_free(node->bap);
		kfree(node);
	}
}

static struct mtb_node *mtb_search(struct rb_root *root, uint8_t *key,
				   size_t key_size)
{
	struct rb_node *node = root->rb_node;
	while (node) {
		struct mtb_node *mt_data =
			container_of(node, struct mtb_node, node);

		int result = __byte_array_compare(key, key_size,
						  mt_data->bap->key->data,
						  mt_data->bap->key->data_size);

		if (result < 0)
			node = node->rb_left;

		else if (result > 0)
			node = node->rb_right;

		else
			return mt_data;
	}
	return NULL;
}

// if returned value < 0 -> error
// if returned value == 0 -> no such key in rb_tree value was inserted
// if returned value > 0 -> such key exists in rb_tree, value was overwritten returned value equals amount of bytes occupied by bap
static int mtb_insert(struct rb_root *root, uint8_t *key, size_t key_size,
		      uint8_t *value, size_t value_size)
{
	int overwritten_bytes = 0;

	struct mtb_node *data =
		mtb_node_create(key, key_size, value, value_size);
	if (!data)
		goto no_mem;

	struct rb_node **new = &(root->rb_node);
	struct rb_node *parent = NULL;

	while (*new) {
		struct mtb_node *this =
			container_of(*new, struct mtb_node, node);

		int result = bap_compare(data->bap, this->bap);

		parent = *new;

		if (result < 0) {
			new = &((*new)->rb_left);
		} else if (result > 0) {
			new = &((*new)->rb_right);
		} else {
			overwritten_bytes = bap_data_size(this->bap);
			rb_replace(this->node, data->node);
			mtb_node_free(this);
		}
	}

	if (overwritten_bytes == 0) {
		rb_link_node(&data->node, parent, new);
		rb_insert_color(&data->node, root);
	}
	return overwritten_bytes;

no_mem:
	return -ENOMEM;
}

/* functions visible from memtable interface */
struct lsm_memtable *lsm_create_memtable()
{
	struct lsm_memtable *new_table = kzalloc(sizeof(*new_table));
	if (!new_table)
		return NULL;

	new_table->tree = RB_ROOT;
	return new_table;
}

void lsm_free_memtable(struct lsm_memtable *table)
{
	struct mtb_node *pos, *node;
	rbtree_postorder_for_each_entry_safe(pos, node, table->root, node) {
		mtb_node_free(pos);
	}
}

void lsm_memtable_remove(struct lsm_memtable *table, uint8_t *key,
			 size_t key_size)
{
	struct mtb_node *dapointerta =
		mtb_search(&(table->root), key, key_size);
	if (data) {
		table->byte_size -= bap_data_size(data->bap);
		rb_erase(data->node, table->root);
		mtb_node_free(data);
	}
}

int lsm_memtable_add(struct lsm_memtable *table, uint8_t *key, size_t key_size,
		     uint8_t *value, size_t value_size)
{
	int overwritten_bytes =
		mtb_insert(&(table->root), key, key_size, value, value_size);

	if (res < 0)
		return res;

	table->byte_size -= overwritten_bytes;
	table->byte_size += key_size + value_size;
	//TODO: flush if too big here or in outer scope?

	return res;
}

struct byte_array *lsm_memtable_get(struct lsm_memtable *table, uint8_t *key,
				    size_t key_size)
{
	struct mtb_node *target = mtb_search(&(table->root), key, key_size);
	if (!target)
		return NULL;
    
	return target->bap->value;
}
