#include "lsm_b_vec.h"

#include <linux/slab.h>

//TODO: port to kernel
struct lsm_b_vec *b_vec_alloc(size_t num_bits)
{
	if (num_bits == 0)
		return NULL;

	struct lsm_b_vec *vec = kzalloc(sizeof(*vec), GFP_KERNEL);
	if (!vec)
		goto allocation_failure;

	size_t data_size = num_bits / BITS_IN_TYPE(DATA_TYPE);
	if (!(num_bits % BITS_IN_TYPE(DATA_TYPE)))
		data_size++;

	vec->data = kzalloc(sizeof(*(vec->data)) * data_size, GFP_KERNEL);
	if (!vec->data)
		goto allocation_failure;
	vec->size = data_size;
	return vec;

allocation_failure:
	b_vec_free(vec);
	return NULL;
}

//lets hope that user is sane and won't use free on NULLPTR
void b_vec_free(struct lsm_b_vec *vec)
{
	if (vec) {
		free(vec->data);
		free(vec);
	}
}

//lets hope that user is sane and won't use get on NULLPTR

_Bool b_vec_get(struct lsm_b_vec *vec, size_t ind)
{
	size_t chunk_offset = ind / BITS_IN_TYPE(DATA_TYPE);
	size_t bit_offset = ind & (BITS_IN_TYPE(DATA_TYPE) - 1);
	DATA_TYPE byte = vec->data[chunk_offset];
	return (byte >> bit_offset) & 1;
}

void b_vec_set(struct lsm_b_vec *vec, size_t ind, _Bool val)
{
	size_t chunk_offset = ind / BITS_IN_TYPE(DATA_TYPE);
	size_t bit_offset = ind & (BITS_IN_TYPE(DATA_TYPE) - 1);
	DATA_TYPE *byte = &(vec->data[chunk_offset]);
	if (val)
		*byte |= ((DATA_TYPE)1) << bit_offset;
	else
		*byte &= ~(1 << bit_offset);
}
