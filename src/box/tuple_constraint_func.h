/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2010-2021, Tarantool AUTHORS, please see AUTHORS file.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct tuple_constraint;

/**
 * Initialize constraint assuming that a func_name identifies a struct func
 * in func_cache.
 */
int
tuple_constraint_func_init(struct tuple_constraint *constr,
			   const char *space_name);

#ifdef __cplusplus
} /* extern "C" */
#endif
