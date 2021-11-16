/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2010-2021, Tarantool AUTHORS, please see AUTHORS file.
 */
#include "tuple_constraint.h"

#include "trivia/util.h"
#include "trivia/str_bank.h"
#include "PMurHash.h"
#include "say.h"
#include "small/region.h"

int
tuple_constraint_noop_check(const struct tuple_constraint *constr,
			    const char *mp_data, const char *mp_data_end)
{
	(void)constr;
	(void)mp_data;
	(void)mp_data_end;
	return 0;
}

void
tuple_constraint_noop_destructor(struct tuple_constraint *constr)
{
	(void)constr;
}

static int
tuple_constraint_def_cmp(const struct tuple_constraint_def *def1,
			 const struct tuple_constraint_def *def2)
{
	int rc;
	if (def1->name_len != def2->name_len)
		return def1->name_len < def2->name_len ? -1 : 1;
	if ((rc = memcmp(def1->name, def2->name, def1->name_len)) != 0)
		return rc;
	if (def1->func_name_len != def2->func_name_len)
		return def1->func_name_len < def2->func_name_len ? -1 : 1;
	if ((rc = memcmp(def1->func_name, def2->func_name,
			 def1->func_name_len)) != 0)
		return rc;
	return 0;
}

int
tuple_constraint_cmp(const struct tuple_constraint *constr1,
		     const struct tuple_constraint *constr2)
{
	return tuple_constraint_def_cmp(&constr1->def, &constr2->def);
}

static uint32_t
tuple_constraint_def_hash_process(const struct tuple_constraint_def *def,
				  uint32_t *ph, uint32_t *pcarry)
{
	PMurHash32_Process(ph, pcarry,
			   def->name, def->name_len);
	PMurHash32_Process(ph, pcarry,
			   def->func_name, def->func_name_len);
	return def->name_len + def->func_name_len;
}

uint32_t
tuple_constraint_hash_process(const struct tuple_constraint *constr,
			      uint32_t *ph, uint32_t *pcarry)
{
	return tuple_constraint_def_hash_process(&constr->def, ph, pcarry);
}

/**
 * Copy constraint definition from @a str to @ dst, allocating strings on
 * string @a bank.
 */
static void
tuple_constraint_def_copy(struct tuple_constraint_def *dst,
			  const struct tuple_constraint_def *src,
			  struct str_bank *bank)
{
	dst->name = str_bank_make(bank, src->name, src->name_len);
	dst->name_len = src->name_len;
	dst->func_name = str_bank_make(bank, src->func_name, src->func_name_len);
	dst->func_name_len = src->func_name_len;
}

/*
 * Calculate needed memory size and allocate a memory block for an array
 * of constraints/constraint_defs by given array of constraint definitions
 * @a defs with @a size; initialize string @a bank with the area allocated
 * for strings.
 * It's a common function for allocation both struct constraint and struct
 * constraint_def, so the size of allocating structs must be passed by
 * @a object_size;
 */
static void *
tuple_constraint_alloc(const struct tuple_constraint_def *defs, size_t count,
		       struct str_bank *bank, size_t object_size)
{
	*bank = str_bank_default();
	STR_BANK_RESERVE_ARR(bank, defs, count, name_len);
	STR_BANK_RESERVE_ARR(bank, defs, count, func_name_len);
	size_t total_size = object_size * count + str_bank_size(bank);
	char *res = xmalloc(total_size);
	str_bank_use(bank, res + object_size * count);
	return res;
}

int
tuple_constraint_def_decode(const char **data,
			    struct tuple_constraint_def **def, uint32_t *count,
			    struct region *region, const char **error)
{
	/* Expected normal form of constraints: {name1=func1, name2=func2..}. */
	if (mp_typeof(**data) != MP_MAP) {
		*error = "constraint field is expected to be a MAP";
		return -1;
	}
	*count = mp_decode_map(data);
	if (*count == 0)
		return 0;

	int bytes;
	*def = region_alloc_array(region, struct tuple_constraint_def,
				  *count, &bytes);
	if (*def == NULL) {
		*error = "array of constraints";
		return bytes;
	}

	for (size_t i = 0; i < *count * 2; i++) {
		if (mp_typeof(**data) != MP_STR) {
			*error = i % 2 == 0 ?
				 "constraint name is expected to be a string" :
				 "constraint func is expected to be a string";
			return -1;
		}
		uint32_t str_len;
		const char *str = mp_decode_str(data, &str_len);
		char *str_copy = region_alloc(region, str_len + 1);
		if (str_copy == NULL) {
			*error = i % 2 == 0 ? "constraint name"
					    : "constraint func";
			return str_len + 1;
		}
		memcpy(str_copy, str, str_len);
		str_copy[str_len] = 0;
		if (i % 2 == 0) {
			(*def)[i / 2].name = str_copy;
			(*def)[i / 2].name_len = str_len;
		} else {
			(*def)[i / 2].func_name = str_copy;
			(*def)[i / 2].func_name_len = str_len;
		}
	}
	return 0;
}

struct tuple_constraint_def *
tuple_constraint_def_collocate(const struct tuple_constraint_def *defs,
			       size_t count)
{
	if (count == 0)
		return NULL;

	struct str_bank bank;
	struct tuple_constraint_def *res;
	res = tuple_constraint_alloc(defs, count, &bank, sizeof(*res));

	/* Now fill the new array. */
	for (size_t i = 0; i < count; i++)
		tuple_constraint_def_copy(&res[i], &defs[i], &bank);

	/* If we did i correctly then there is no more space for strings. */
	assert(str_bank_size(&bank) == 0);
	return res;
}

struct tuple_constraint *
tuple_constraint_collocate(const struct tuple_constraint_def *defs,
			   size_t count)
{
	if (count == 0)
		return NULL;

	struct str_bank bank;
	struct tuple_constraint *res;
	res = tuple_constraint_alloc(defs, count, &bank, sizeof(*res));

	/* Now fill the new array. */
	for (size_t i = 0; i < count; i++) {
		tuple_constraint_def_copy(&res[i].def, &defs[i], &bank);
		res[i].check = tuple_constraint_noop_check;
		res[i].check_ctx = NULL;
		res[i].destroy = tuple_constraint_noop_destructor;
	}

	/* If we did i correctly then there is no more space for strings. */
	assert(str_bank_size(&bank) == 0);
	return res;

}