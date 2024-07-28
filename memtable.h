#ifndef LSBLK_MEMTABLE_H
#define LSBLK_MEMTABLE_H

#include <linux/rbtree.h>
#include <linux/types.h>

struct mtb_node {
	struct rb_node node;
	sector_t logical_addr;
	sector_t physical_addr;
};

struct lsm_memtable {
	struct rb_root tree;
	uint64_t byte_size;
};

struct lsm_memtable *lsm_create_memtable(void);

void lsm_free_memtable(struct lsm_memtable *table);

int lsm_memtable_add(struct lsm_memtable *table, sector_t logical_addr,
		     sector_t physical_addr);

void lsm_memtable_remove(struct lsm_memtable *table, sector_t logical_addr);

//TODO: return reference or return copy?
//returns reference
struct mtb_node *lsm_memtable_get(struct lsm_memtable *table,
				  sector_t logical_addr);

#endif
