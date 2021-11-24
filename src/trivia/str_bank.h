/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2010-2021, Tarantool AUTHORS, please see AUTHORS file.
 */
#pragma once

#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * String bank is a data structure that is designed for simplification of
 * allocation of several strings in one memory block.
 * Each string is allocated with a personal null termination symbol.
 * Typical usage consist of two phases: gathering total needed size of memory
 * block and creation of strings in given block.
 */
struct str_bank {
	char *data;
	char *data_end;
};

static inline struct str_bank
str_bank_default()
{
	struct str_bank res = {NULL, NULL};
	return res;
}

static inline void
str_bank_reserve(struct str_bank *bank, size_t size)
{
	bank->data_end += size + 1;
}

#define STR_BANK_RESERVE_ARR(bank, arr, arr_size, size_member)			\
do {										\
	for (size_t i = 0; i < (arr_size); i++)					\
		str_bank_reserve(bank, (arr)[i].size_member);			\
} while(0)

static inline size_t
str_bank_size(struct str_bank *bank)
{
	return bank->data_end - bank->data;
}

static inline void
str_bank_use(struct str_bank *bank, char *data)
{
	bank->data_end = data + (bank->data_end - bank->data);
	bank->data = data;
}

static inline char *
str_bank_make(struct str_bank *bank, const char *src, size_t src_size)
{
	char *res = bank->data;
	memcpy(res, src, src_size);
	res[src_size] = 0;
	bank->data += src_size + 1;
	return res;
}

#ifdef __cplusplus
} /* extern "C" */
#endif
