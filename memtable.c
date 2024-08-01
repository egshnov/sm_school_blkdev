#include "memtable.h"
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>

//TODO: ADD CONDITIONAL COMPILATION FOR SKIP_LIST WHEN READY
/*struct for wrapping kv - pairs to use with kernels rb_tree*/

static struct mtb_node *mtb_node_create(sector_t logical_addr,
					sector_t physical_addr)
{
	struct mtb_node *node;

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node)
		return NULL;
	node->logical_addr = logical_addr;
	node->physical_addr = physical_addr;

	return node;
}

static void mtb_node_free(struct mtb_node *node)
{
	kfree(node);
}

static int compare_addr(sector_t lhs, sector_t rhs)
{
	return lhs < rhs ? -1 : (lhs == rhs ? 0 : 1);
}
static struct mtb_node *mtb_search(struct rb_root *root, sector_t logical_addr)
{
	struct rb_node *node;

	node = root->rb_node;
	while (node) {
		struct mtb_node *mt_data =
			container_of(node, struct mtb_node, node);

		int result = compare_addr(logical_addr, mt_data->logical_addr);

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
static int mtb_insert(struct rb_root *root, sector_t logical_addr,
		      sector_t physical_addr)
{
	int overwritten_bytes;
	struct rb_node **new;
	struct rb_node *parent;
	struct mtb_node *data;
	struct mtb_node *this;
	int result;

	overwritten_bytes = 0;
	new = &(root->rb_node);
	parent = NULL;

	while (*new) {
		this = container_of(*new, struct mtb_node, node);

		result = compare_addr(logical_addr, this->logical_addr);

		parent = *new;

		if (result < 0) {
			new = &((*new)->rb_left);
		} else if (result > 0) {
			new = &((*new)->rb_right);
		} else {
			overwritten_bytes = sizeof(sector_t) * 2;
			this->physical_addr = physical_addr;
			return overwritten_bytes;
		}
	}

	if (overwritten_bytes == 0) {
		data = mtb_node_create(logical_addr, physical_addr);
		if (!data)
			goto no_mem;
		rb_link_node(&data->node, parent, new);
		rb_insert_color(&data->node, root);
	}
	return overwritten_bytes;

no_mem:
	return -ENOMEM;
}

/* functions visible from memtable interface */
struct lsm_memtable *lsm_create_memtable(void)
{
	struct lsm_memtable *new_table;

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return NULL;

	new_table->tree = RB_ROOT;
	return new_table;
}

void lsm_free_memtable(struct lsm_memtable *table)
{
	struct mtb_node *pos, *node;
	rbtree_postorder_for_each_entry_safe(pos, node, &(table->tree), node) {
		mtb_node_free(pos);
	}
}

void lsm_memtable_remove(struct lsm_memtable *table, sector_t logical_addr)
{
	struct mtb_node *data;

	data = mtb_search(&(table->tree), logical_addr);
	if (data) {
		table->byte_size -= sizeof(sector_t) * 2;
		rb_erase(&(data->node), &(table->tree));
		mtb_node_free(data);
	}
}

int lsm_memtable_add(struct lsm_memtable *table, sector_t logical_addr,
		     sector_t physical_addr)
{
	int overwritten_bytes;
	
	overwritten_bytes =
		mtb_insert(&(table->tree), logical_addr, physical_addr);

	if (overwritten_bytes < 0)
		return overwritten_bytes;

	table->byte_size -= overwritten_bytes;
	table->byte_size += sizeof(sector_t) * 2;
	//TODO: flush if too big here or in outer scope?

	return overwritten_bytes;
}

struct mtb_node *lsm_memtable_get(struct lsm_memtable *table,
				  sector_t logical_addr)
{
	struct mtb_node *target;
	target = mtb_search(&(table->tree), logical_addr);
	return target;
}
