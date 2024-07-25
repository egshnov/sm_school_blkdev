#ifndef LSMTREE_LSM_B_VEC_H
#define LSMTREE_LSM_B_VEC_H

#include<linux/types.h>


#define BITS_IN_BYTE 8
#define DATA_TYPE uint32_t //TODO: redundant?
#define BITS_IN_TYPE(type) (BITS_IN_BYTE * (sizeof(type)))

//TODO: make struct a macro?
struct lsm_b_vec {
	DATA_TYPE *data;
	size_t size;
};

struct lsm_b_vec *b_vec_alloc(size_t num_bits);

void b_vec_free(struct lsm_b_vec *vec);

bool b_vec_get(struct lsm_b_vec *vec, size_t ind);

void b_vec_set(struct lsm_b_vec *vec, size_t bit_ind, _Bool val);

#endif //LSMTREE_LSM_B_VEC_H
