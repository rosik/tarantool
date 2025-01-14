/*
 * Copyright 2010-2016, Tarantool AUTHORS, please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "coll_id_cache.h"
#include "coll_id.h"
#include "diag.h"
#include "assoc.h"

/** mhash table (name, len -> collation) */
static struct mh_strnptr_t *coll_cache_name = NULL;
/** mhash table (id -> collation) */
static struct mh_i32ptr_t *coll_id_cache = NULL;

void
coll_id_cache_init(void)
{
	coll_id_cache = mh_i32ptr_new();
	coll_cache_name = mh_strnptr_new();
}

void
coll_id_cache_destroy(void)
{
	mh_strnptr_delete(coll_cache_name);
	mh_i32ptr_delete(coll_id_cache);
}

int
coll_id_cache_replace(struct coll_id *coll_id, struct coll_id **replaced_id)
{
	const struct mh_i32ptr_node_t id_node = {coll_id->id, coll_id};
	struct mh_i32ptr_node_t repl_id_node = {0, NULL};
	struct mh_i32ptr_node_t *prepl_id_node = &repl_id_node;
	mh_i32ptr_put(coll_id_cache, &id_node, &prepl_id_node, NULL);

	uint32_t hash = mh_strn_hash(coll_id->name, coll_id->name_len);
	const struct mh_strnptr_node_t name_node =
		{ coll_id->name, coll_id->name_len, hash, coll_id };
	struct mh_strnptr_node_t repl_name_node = { NULL, 0, 0, NULL };
	struct mh_strnptr_node_t *prepl_node_name = &repl_name_node;
	mh_strnptr_put(coll_cache_name, &name_node, &prepl_node_name, NULL);
	assert(repl_id_node.val == repl_name_node.val);
	assert(repl_id_node.val == NULL);
	*replaced_id = repl_id_node.val;
	return 0;
}

void
coll_id_cache_delete(const struct coll_id *coll_id)
{
	mh_int_t id_i = mh_i32ptr_find(coll_id_cache, coll_id->id, NULL);
	mh_i32ptr_del(coll_id_cache, id_i, NULL);
	mh_int_t name_i = mh_strnptr_find_inp(coll_cache_name, coll_id->name,
					      coll_id->name_len);
	mh_strnptr_del(coll_cache_name, name_i, NULL);
}

struct coll_id *
coll_by_id(uint32_t id)
{
	mh_int_t pos = mh_i32ptr_find(coll_id_cache, id, NULL);
	if (pos == mh_end(coll_id_cache))
		return NULL;
	return mh_i32ptr_node(coll_id_cache, pos)->val;
}

/**
 * Find a collation object by its name.
 */
struct coll_id *
coll_by_name(const char *name, uint32_t len)
{
	mh_int_t pos = mh_strnptr_find_inp(coll_cache_name, name, len);
	if (pos == mh_end(coll_cache_name))
		return NULL;
	return mh_strnptr_node(coll_cache_name, pos)->val;
}
