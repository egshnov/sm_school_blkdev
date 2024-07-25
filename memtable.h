#ifndef LSBLK_MEMTABLE_H
#define LSBLK_MEMTABLE_H

#include <linux/rbtree.h>
#include <linux/types.h>

struct byte_array {
	uint8_t *data;
	size_t data_size;
};

struct *byte_array byte_array_create(uint8_t *data, size_t data_size);

void byte_array_free(struct byte_array *arr);

int byte_array_compare(struct byte_array *lhs, struct byte_array *rhs);

struct lsm_memtable {
	struct rb_root tree;
	uint64_t byte_size;
};

struct lsm_memtable *lsm_create_memtable();

void lsm_free_memtable(struct lsm_memtable *table);

int lsm_memtable_add(struct lsm_memtable *table, uint8_t *key, size_t key_size,
		     uint8_t *value, size_t value_size);

void lsm_memtable_remove(struct lsm_memtable *table, uint8_t *key,
			 size_t key_size);

//TODO: return reference or return copy?
//returns reference
struct byte_array *lsm_memtable_get(struct lsm_memtable *table, uint8_t *key,
				    size_t key_size);

#endif
