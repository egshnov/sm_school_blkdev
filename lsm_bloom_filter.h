#ifndef LSBLK_BLFILTER_H
#define LSBLK_BLFILTER_H

struct lsm_bloom_filter {
	struct lsm_b_vec *vec;
	size_t num_hash_func;
	size_t num_items;
};

struct lsm_bloom_filter *bloom_filter_alloc(size_t size, size_t num_functions);

void bloom_filter_free(struct lsm_bloom_filter *filter);

void bloom_filter_put(struct lsm_bloom_filter *filter, const void *data,
		      size_t length);

void bloom_filter_put_str(struct lsm_bloom_filter *filter, const char *str);

_Bool bloom_filter_test(struct lsm_bloom_filter *filter, const void *data,
			size_t length);

_Bool bloom_filter_test_str(struct lsm_bloom_filter *filter, const char *str);
#endif
