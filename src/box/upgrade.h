#pragma once
/*
 * Copyright 2010-2021, Tarantool AUTHORS, please see AUTHORS file.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

struct func;
struct space;
struct tuple;

/** Status of upgrade operation. */
enum upgrade_status {
	/**
	 * Upgrade has been launched: upgrade options are verified,
	 * insertion to _space_upgrade has been processed, space's
	 * format is updated to new (if any).
	 */
	UPGRADE_INPROGRESS = 0,
	/**
	 * Set in case in-progress upgrade fails for whatever reason.
	 * User is supposed to update upgrade function and/or set the new
	 * format and re-run upgrade.
	 */
	UPGRADE_ERROR = 1,
	/**
	 * Set if space to be upgraded is tested with given upgrade function
	 * and/or new format. No real-visible data changes occur.
	 * */
	UPGRADE_TEST = 2,
};

/** Structure incorporating all vital details concerning upgrade operation. */
struct space_upgrade {
	/**
	 * Id of the space being upgraded. Used to identify space
	 * in on_commit/on_rollback triggers which are set in
	 * on_replace_dd_space_upgrade.
	 */
	uint32_t space_id;
	/** Status of current upgrade. */
	enum upgrade_status status;
	/** Pointer to the upgrade function. */
	struct func *func;
	/**
	 * New format of the space. It is used only in TEST mode; during
	 * real upgrade space already features updated format
	 * (space->tuple_format).
	 */
	struct tuple_format *format;
};

extern const char *upgrade_status_strs[];

static inline enum upgrade_status
upgrade_status_by_name(const char *name, uint32_t name_len)
{
	if (name_len == 10 && strncmp(name, "inprogress",
				      strlen("inprogress")) == 0)
		return UPGRADE_INPROGRESS;
	if (name_len == 4  && strncmp(name, "test", strlen("test")) == 0)
		return UPGRADE_TEST;
	if (name_len == 5  && strncmp(name, "error", strlen("error")) == 0)
		return UPGRADE_ERROR;
	unreachable();
	return UPGRADE_ERROR;
}

/** Functions below are used in alter.cc so surround them with C++ guards. */
#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

void
space_upgrade_delete(struct space_upgrade *upgrade);

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */

/**
 * Launch test run of upgrade: it does not modify data; only verifies that
 * tuples after upgrade met all required conditions.
 */
int
space_upgrade_test(uint32_t space_id);

/** Launch upgrade operation. */
int
space_upgrade(uint32_t space_id);
