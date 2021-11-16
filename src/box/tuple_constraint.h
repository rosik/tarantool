/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2010-2021, Tarantool AUTHORS, please see AUTHORS file.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "func_cache.h"

#ifdef __cplusplus
extern "C" {
#endif

struct tuple_constraint;

/**
 * Type of constraint check function. Must return 0 if constraint passed.
 */
typedef int (*tuple_constraint_f)(const struct tuple_constraint *constraint,
				  const char *mp_data, const char *mp_data_end);

/**
 * Type of constraint destructor.
 */
typedef void (*tuple_constraint_destroy_f)(struct tuple_constraint *constr);

/**
 * Generic constraint of a tuple or a tuple field field.
 */
struct tuple_constraint_def {
	/** Name of the constraint (null-terminated). */
	const char *name;
	/** Name of function to check the constraint (null-terminated). */
	const char *func_name;
	/** Length of name of the constraint. */
	uint32_t name_len;
	/** Length of name of function to check the constraint. */
	uint32_t func_name_len;
};

/**
 * Generic constraint of a tuple or a tuple field.
 */
struct tuple_constraint {
	/** Constraint definition. */
	struct tuple_constraint_def def;
	/** The constraint function itself. */
	tuple_constraint_f check;
	/** User-defined context that can be used by function. */
	void *check_ctx;
	/**
	 * Destructor. Should be called when the object in disposed.
	 * Designed to be reentrant - it's ok to call it twice.
	 */
	tuple_constraint_destroy_f destroy;
	/** Various data for different state of constraint. */
	union {
		/** Data of pinned function in func cache. */
		struct func_cache_holder func_cache_holder;
		/** Data of subscription in func cache. */
		struct func_cache_subscription func_cache_subscription;
	};
};

/**
 * Check that checks nothing and always passes. Used as a default.
 */
int
tuple_constraint_noop_check(const struct tuple_constraint *constraint,
			    const char *mp_data, const char *mp_data_end);

/**
 * No-op destructor of constraint. Used as a default.
 */
void
tuple_constraint_noop_destructor(struct tuple_constraint *constr);

/**
 * Compare two constraint objects.
 * Return 0 if they are equal in terms of string names.
 * Don't compare function pointers.
 */
int
tuple_constraint_cmp(const struct tuple_constraint *constr1,
		     const struct tuple_constraint *constr2);

/**
 * Append tuple constraint to hash calculation using PMurHash32_Process.
 * Process only string names, don't take function pointers into consideration.
 * Return size of processed data.
 */
uint32_t
tuple_constraint_hash_process(const struct tuple_constraint *constr,
			      uint32_t *ph, uint32_t *pcarry);

/**
 * Parse constraint array from msgpack @a *data with the following format:
 * {constraint_name=function_name,...}
 * Allocate a temporary constraint array on @a region and save it in @a def.
 * Allocate needed for constraints strings also on @a region.
 * Set @a count to the count of parsed constraints.
 * Move @a data msgpack pointer to the end of msgpack value.
 *
 * Return:
 *   0 - success.
 *  >0 - that number of bytes was failed to allocate on region
 *   (@a error is set to allocation description)
 *  -1 - format error (@a error is set to description).
 */
int
tuple_constraint_def_decode(const char **data,
			   struct tuple_constraint_def **def, uint32_t *count,
			   struct region *region, const char **error);

/**
 * Allocate a single memory block with array of constraint defs.
 * The memory block starts immediately with tuple_constraint_def array and then
 * is followed by strings block with strings that each constraint refers to.
 * This memory block must be freed by free() call.
 * Never fails (uses xmalloc) and returns NULL if constraint_count == 0.
 *
 * @param def - array of given constraint definitions.
 * @param constraint_count - number of give constraints.
 * @return a single memory block with constraints.
 */
struct tuple_constraint_def *
tuple_constraint_def_collocate(const struct tuple_constraint_def *defs,
			       size_t count);

/**
 * Allocate a single memory block with array of constraints.
 * Similar to tuple_constraint_def_collocate, but creates constraints.
 * Functions (check and destroy) are set to defaults.
 *
 * @param def - array of given constraint definitions.
 * @param constraint_count - number of give constraints.
 * @return a single memory block with constraints.
 */
struct tuple_constraint *
tuple_constraint_collocate(const struct tuple_constraint_def *def,
			   size_t count);

#ifdef __cplusplus
} /* extern "C" */
#endif
