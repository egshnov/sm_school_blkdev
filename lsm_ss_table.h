#ifndef LSM_SS_TABLE_H
#define LSM_SS_TABLE_H

#include "lsm_bloom_filter.h"
#include "memtable.h"
#define DATA_FILE_EXTENSION ".data"
#define BLOOM_FILE_EXTENSION ".bloom"
#define INDEX_FILE_EXTENSION ".index"

//TODO: struct sparse_index = key + offset of kv-pair

struct lsm_ss_table {
	char *filename;
	//something like filestream?
	struct lsm_bloom_filter *filter;

	//TODO: list of offsets
	//TODO: sparse size count
	//TODO: list of sparse_keys

	struct byte_array *min_key;
	struct byte_array *max_key;
};

//TODO: iterate through

struct lsm_ss_table *ss_table_from_memtable(struct lsm_memtable *memtable);

struct lsm_ss_table *ss_table_from_file(char *filename);

struct byte_array *get(struct lsm_ss_table *table, struct byte_array *key);

void merge_ss_tables(struct lsm_ss_table *lhs, struct lsm_ss_table *rhs);

void ss_table_delete_files(struct lsm_ss_table *table);

void ss_table_close(struct lsm_ss_table *table);

#endif
