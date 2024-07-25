#include "lsm_bloom_filter.h"

#include <linux/string.h>


struct lsm_bloom_filter *bloom_filter_alloc(size_t size, size_t num_hash_func)
{
	struct lsm_bloom_filter *filter = kzalloc(sizeof(*filter), GFP_KERNEL);
	if (!filter)
		return NULL;
	filter->vec = b_vec_alloc(size);
	if (!filter->vec)
		goto dealloc;

	filter->num_items = 0;
	filter->num_hash_func = num_hash_func;
	return filter;

dealloc:
	free(filter);
	return NULL;
}

void bloom_filter_free(struct lsm_bloom_filter *filter)
{
	if (filter) {
		b_vec_free(filter->vec);
		free(filter);
	}
}

//TODO: CHANGE HASHING!!!!!!!!!

void bloom_filter_put(struct lsm_bloom_filter *filter, const void *data,
		      size_t length)
{
	//TODO: sanity check?
	for (int i = 0; i < filter->num_hash_func; i++) {
		DATA_TYPE cur_hash = murmurhash(data, length, i);
		b_vec_set(filter->vec, cur_hash % filter->vec->size, 1);
	}
	filter->num_items++;
}

void bloom_filter_put_str(struct lsm_bloom_filter *filter, const char *str)
{
	//TODO: sanity check?
	bloom_filter_put(filter, str, strlen(str));
}

_Bool bloom_filter_test(struct lsm_bloom_filter *filter, const void *data,
			size_t length)
{
	for (int i = 0; i < filter->num_hash_func; i++) {
		DATA_TYPE cur_hash = murmurhash(data, length, i);
		if (!b_vec_get(filter->vec, cur_hash % filter->vec->size))
			return false;
	}
	return true;
}

_Bool bloom_filter_test_str(struct lsm_bloom_filter *filter, const char *str)
{
	return bloom_filter_test(filter, str, strlen(str));
}
