#ifndef LSBLK_MEMTABLE_H
#define LSBLK_MEMTABLE_H

#include <linux/rbtree.h>
#include <linux/types.h>

struct lsm_memtable {
	struct rb_root tree;
	uint64_t byte_size;
};

struct lsm_memtable *lsm_create_memtable();

void lsm_free_memtable(struct lsm_memtable *table);

void lsm_memtable_add(uint8_t *key, size_t key_size, uint8_t *value,
		      size_t value_size);

void lsm_memtable_remove(uint8_t *key, size_t key_size, uint8_t *value,
			 size_t value_size);

#endif
