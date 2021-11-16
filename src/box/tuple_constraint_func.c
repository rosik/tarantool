/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2010-2021, Tarantool AUTHORS, please see AUTHORS file.
 */

#include "tuple_constraint_func.h"

#include "engine.h"
#include "func.h"
#include "func_cache.h"
#include "port.h"
#include "tuple_constraint.h"
#include "tt_static.h"
#include "trivia/util.h"

/**
 * Find and verify func in func cache.
 * Return NULL if something went wrong (diag is set).
 */
struct func *
tuple_constraint_func_find(struct tuple_constraint *constr,
			   const char *space_name)
{
	const char *func_name = constr->def.func_name;
	struct func *func = func_by_name(func_name, constr->def.func_name_len);
	if (func == NULL)
		diag_set(ClientError, ER_CREATE_CONSTRAINT,
			 constr->def.name, space_name,
			 tt_sprintf("constraint lua function '%s' "
				    "was not found by name",
				    func_name));
	return func;
}

/**
 * Verify function @a func comply function rules for constraint @a constr.
 * Return nonzero in case on some problems, diag is set (@a space_name is
 * used for better error message).
 */
static int
tuple_constraint_func_verify(struct tuple_constraint *constr,
			     struct func *func, const char *space_name)
{
	const char *func_name = constr->def.func_name;

	if (func->def->language == FUNC_LANGUAGE_LUA &&
	    func->def->body == NULL) {
		diag_set(ClientError, ER_CREATE_CONSTRAINT,
			 constr->def.name, space_name,
			 tt_sprintf("constraint lua function '%s' "
				    "must have persistent body",
				    func_name));
		return -1;
	}
	if (!func->def->is_deterministic) {
		diag_set(ClientError, ER_CREATE_CONSTRAINT,
			 constr->def.name, space_name,
			 tt_sprintf("constraint function '%s' "
				    "must be deterministic",
				    func_name));
		return -1;
	}
	return 0;
}

/**
 * Constraint check function that interpret constraint->func_ctx as a pointer
 * to struct func and call it.
 * @param constraint that is checked.
 * @param mp_data, @param mp_data_end - pointers to msgpack data (of a field
 *  or entire tuple, depending on constraint.
 * @return 0 - constraint is passed, not 0 - constraint is failed.
 */
static int
tuple_constraint_call_func(const struct tuple_constraint *constr,
			   const char *mp_data, const char *mp_data_end)
{
	struct port out_port, in_port;
	port_c_create(&in_port);
	port_c_add_mp(&in_port, mp_data, mp_data_end);
	struct func *func = (struct func *)constr->check_ctx;
	int rc = func_call(func, &in_port, &out_port);
	port_destroy(&in_port);
	if (rc == 0)
		port_destroy(&out_port);
	return rc;
}

/**
 * Destructor that unpins func from func_cache.
 */
static void
tuple_constraint_func_unpin(struct tuple_constraint *constr)
{
	assert(constr->destroy == tuple_constraint_func_unpin);
	struct func *func = (struct func *)constr->check_ctx;
	func_cache_unpin(func, &constr->func_cache_holder);
	constr->check = tuple_constraint_noop_check;
	constr->check_ctx = NULL;
	constr->destroy = tuple_constraint_noop_destructor;
}

/**
 * Destructor that unsubscribes from func_cache.
 */
static void
tuple_constraint_func_unsubsribe(struct tuple_constraint *constr)
{
	assert(constr->destroy == tuple_constraint_func_unsubsribe);
	func_cache_unsubscribe_by_name(constr->def.func_name,
				       constr->def.func_name_len,
				       &constr->func_cache_subscription);
	constr->check = tuple_constraint_noop_check;
	constr->check_ctx = NULL;
	constr->destroy = tuple_constraint_noop_destructor;
}

/**
 * Function for subscription on addition of function to func cache.
 * When the function is added to func cache, it the corresponding constraint
 * is initialized with it in usual way.
 */
static void
tuple_constraint_func_subscr(struct func_cache_subscription *sub,
			     struct func *func)
{
	struct tuple_constraint *constr = container_of(sub,
						       struct tuple_constraint,
						       func_cache_subscription);
	if (tuple_constraint_func_verify(constr, func, "unknown") != 0)
		panic("Failed to load constraint function during recovery!");
	func_cache_pin(func, &constr->func_cache_holder,
		       HOLDER_TYPE_CONSTRAINT);
	constr->check = tuple_constraint_call_func;
	constr->check_ctx = func;
	constr->destroy = tuple_constraint_func_unpin;
}

int
tuple_constraint_func_init(struct tuple_constraint *constr,
			   const char *space_name)
{
	if (engine_is_in_initial_recovery()) {
		func_cache_subscribe_by_name(constr->def.func_name,
					     constr->def.func_name_len,
					     &constr->func_cache_subscription,
					     tuple_constraint_func_subscr);
		constr->destroy = tuple_constraint_func_unsubsribe;
	} else {
		struct func *func = tuple_constraint_func_find(constr,
							       space_name);
		if (func == NULL)
			return -1;
		if (tuple_constraint_func_verify(constr, func, space_name) != 0)
			return -1;
		func_cache_pin(func, &constr->func_cache_holder,
			       HOLDER_TYPE_CONSTRAINT);
		constr->check = tuple_constraint_call_func;
		constr->check_ctx = func;
		constr->destroy = tuple_constraint_func_unpin;
	}
	return 0;
}
