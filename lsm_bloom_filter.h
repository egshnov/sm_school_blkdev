#ifndef LSBLK_BLFILTER_H
#define LSBLK_BLFILTER_H

#include "lsm_b_vec.h"

struct lsm_bloom_filter {
	struct lsm_b_vec *vec;
	size_t num_hash_func;
	size_t num_items;
};

/*TODO: обусдить требуемый размер блум фильтра и желаемы false_positive_rate
  передавать в функцию значения полученные по формулам:
  size = (-expectedInsertions * log(falsePositiveRate) / (log(2) * log(2)));
  num_functions =  ceil(-log(falsePositiveRate) / log(2));
  самостоятельно этого не сделать т.к. в кернеле не получится использовать floating point operations
  */
  
struct lsm_bloom_filter *bloom_filter_alloc(size_t size, size_t num_functions);

void bloom_filter_free(struct lsm_bloom_filter *filter);

void bloom_filter_put(struct lsm_bloom_filter *filter, const void *data,
		      size_t length);

void bloom_filter_put_str(struct lsm_bloom_filter *filter, const char *str);

_Bool bloom_filter_test(struct lsm_bloom_filter *filter, const void *data,
			size_t length);

_Bool bloom_filter_test_str(struct lsm_bloom_filter *filter, const char *str);
#endif
