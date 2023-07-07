// SPDX-License-Identifier: LGPL-2.0
/*
 * General utilities
 *
 * This file comprises helpers to aid with generic utilities throughout
 * the software.
 *
 * Copyright (C) 2023 Rajat Khandelwal <rajat.khandelwal@intel.com>
 * Copyright (C) 2023 Intel Corporation
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>

#include "utils.h"

#define GET_ALIGNED_PAGE(x, a)		_GET_ALIGNED_PAGE(x, (typeof(x))(a) - 1)
#define _GET_ALIGNED_PAGE(x, a)		(((x) + (a)) & ~(a))

/* Control packets */
#define CRC32_POLY_LE			0x82f63b78

/* Transport packet header */
#define CRC8_POLY			0x07
#define CRC8_XOROUT			0x55

/* TBT control packets use 32-bit CRC */
static u32 crc32_table_le[4][256];
static u32 *crc32_t0, *crc32_t1, *crc32_t2, *crc32_t3;

static bool is_page_aligned(u64 off)
{
	return !off || ((PAGE_SIZE % off) == 0);
}

static bool is_arch_x86(void)
{
	char *path = "uname -m | grep x86 | wc -l";
	char *bash_result;
	bool ret;

	bash_result = do_bash_cmd(path);
	ret = strtoud(bash_result);

	free(bash_result);

	return ret;
}

static bool is_cpu_le(void)
{
	char *path = "lscpu | grep 'Little Endian' | wc -l";
	char *bash_result;
	bool ret;

	bash_result = do_bash_cmd(path);
	ret = strtoud(bash_result);

	free(bash_result);

	return ret;
}

static u32 do_crc(u32 crc, u32 x)
{
	if (is_cpu_le())
		return crc32_t0[(crc ^ (x)) & 255] ^ (crc >> 8);
	else
		return crc32_t0[((crc >> 24) ^ (x)) & 255] ^ (crc << 8);
}

static u32 do_crc4(u32 q)
{
	if (is_cpu_le())
		return (crc32_t3[(q) & 255] ^ crc32_t2[(q >> 8) & 255] ^ \
			crc32_t1[(q >> 16) & 255] ^ crc32_t0[(q >> 24) & 255]);
	else
		return (crc32_t0[(q) & 255] ^ crc32_t1[(q >> 8) & 255] ^ \
			crc32_t2[(q >> 16) & 255] ^ crc32_t3[(q >> 24) & 255]);
}

static void crc32_init(void)
{
	u32 crc = 1;
	u32 i, j;

	crc32_table_le[0][0] = 0;

	for (i = 256 >> 1; i; i >>= 1) {
		crc = (crc >> 1) ^ ((crc & 1) ? CRC32_POLY_LE : 0);

		for (j = 0; j < 256; j += 2 * i)
			crc32_table_le[0][i + j] = crc ^ crc32_table_le[0][j];
	}

	for (i = 0; i < 256; i++) {
		crc = crc32_table_le[0][i];

		for (j = 1; j < 4; j++) {
			crc = crc32_table_le[0][crc & 0xff] ^ (crc >> 8);
			crc32_table_le[j][i] = crc;
		}
	}

	crc32_t0 = crc32_table_le[0];
	crc32_t1 = crc32_table_le[1];
	crc32_t2 = crc32_table_le[2];
	crc32_t3 = crc32_table_le[3];
}

struct list_item* list_add(struct list_item *tail, void *val)
{
	struct list_item *temp = malloc(sizeof(struct list_item));

	temp->val = val;
	temp->next = NULL;

	if (tail == NULL)
		return temp;

	tail->next = temp;

	return temp;
}

int strpos(const char *str, const char *substr, u64 offset)
{
	char *pos_str;
	int pos;

	pos_str = strstr(str + offset, substr);

	if (pos_str)
		pos = pos_str - str;
	else
		pos = -1;

	return pos;
}

char* do_bash_cmd(const char *cmd)
{
	char *output = malloc(MAX_LEN * sizeof(char));
	FILE *file = popen(cmd, "r");
	char *ret;

	ret = fgets(output, MAX_LEN, file);
	if (!ret) {
		pclose(file);
		free(output);

		return ret;
	}

	pclose(file);

	return trim_white_space(output);
}

struct list_item* do_bash_cmd_list(const char *cmd)
{
	char *output = malloc(MAX_LEN * sizeof(char));
	struct list_item *head = NULL;
	struct list_item *tail = head;
	FILE *file = popen(cmd, "r");
	char *temp_output = NULL;

	while (fgets(output, MAX_LEN, file) != NULL) {
		temp_output = malloc(MAX_LEN * sizeof(char));

		output = trim_white_space(output);

		if (tail == NULL) {
			head = list_add(tail, (void*)output);
			tail = head;

			output = temp_output;
			continue;
		}

		tail = list_add(tail, (void*)output);

		output = temp_output;
	}

	pclose(file);

	free(output);

	return head;
}

char* trim_white_space(char *str)
{
	char *end;

	while (isspace((unsigned char)*str))
		str++;

	if (*str == 0)
		return str;

	end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end))
		end--;

	*++end = '\0';

	return str;
}

char* switch_cmd_to_root(const char *cmd)
{
	char *cmd_to_return = malloc(MAX_LEN * sizeof(char));
	char cmd_as_root[MAX_LEN];

	snprintf(cmd_as_root, sizeof(cmd_as_root), "sudo bash -c \"%s\"", cmd);

	strncpy(cmd_to_return, cmd_as_root, MAX_LEN);
	return trim_white_space(cmd_to_return);
}

u64 get_page_aligned_addr(u64 off)
{
	if (is_page_aligned(off))
		return off;

	return GET_ALIGNED_PAGE(off, PAGE_SIZE);
}

void* get_user_mapped_read_va(int fd, u64 off, u64 size)
{
	if (fd == -1)
		return mmap(NULL, size, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

	return mmap(NULL, size, PROT_READ, MAP_SHARED, fd, off);
}

void* get_user_mapped_write_va(int fd, u64 off, u64 size)
{
	if (fd == -1)
		return mmap(NULL, size, PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

	return mmap(NULL, size, PROT_WRITE, MAP_SHARED, fd, off);
}

void* get_user_mapped_rw_va(int fd, u64 off, u64 size)
{
	if (fd == -1)
		return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

	return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, off);
}

void unmap_user_mapped_va(void *addr, u64 size)
{
	munmap(addr, size);
}

u64 get_size_least_set(u64 bitmask)
{
	return (u64)1 << (ffsll(bitmask) - 1);
}

u32 get_crc32(u32 crc, const u8 *data, u64 size)
{
	u64 rem_size;
	u32 *b, q;
	u64 i;

	crc32_init();

	if ((long)data & 3 && size) {
		do {
			crc = do_crc(crc, *data++);
		} while ((--size) && ((long)data) & 3);
	}

	rem_size = size & 3;
	size = size >> 2;

	b = (u32*)data;

	if (is_arch_x86()) {
		--b;

		for (i = 0; i < size; i++) {
			q = crc ^ *++b;
			crc = do_crc4(q);
		}

		size = rem_size;

		if (size) {
			u8 *p = (u8*)(b + 1) - 1;

			for (i = 0; i < size; i++)
				crc = do_crc(crc, *++p);
		}
	} else {
		for (--b; size; --size) {
			q = crc ^ *++b;
			crc = do_crc4(q);
		}

		size = rem_size;

		if (size) {
			u8 *p = (u8*)(b + 1) - 1;

			do {
				crc = do_crc(crc, *++p);
			} while (--size);
		}
	}

	return crc;
}

u8 get_crc8(u8 crc, const u8 *data, u64 size)
{
	u8 i;

	while (size--) {
		crc ^= *data++;

		for (i = 0; i < 8; i++) {
			if (crc & 0x80)
				crc = (crc << 1) ^ CRC8_POLY;
			else
				crc <<=1;
		}
	}

	return crc ^ CRC8_XOROUT;
}

void convert_to_be32(u32 *data, u64 len)
{
	u64 i;

	for (i = 0; i < len; i++) {
		*data = htobe32(*data);
		++data;
	}
}

void be32_to_u32(u32 *data, u64 len)
{
	u64 i;

	for (i = 0; i < len; i++) {
		*data = be32toh(*data);
		++data;
	}
}

/* Convert a string literal into its equivalent decimal format */
u32 strtoud(char *str)
{
	return strtoul(str, &str, 10);
}

/* Convert a string literal into its equivalent hexadecimal format */
u32 strtouh(char *str)
{
	return strtoul(str, &str, 16);
}

/* Returns a substring from a string 'str', starting from 'pos' with length 'len' */
char* get_substr(const char *str, u64 pos, u64 len)
{
	char *substr = malloc((len + 1)* sizeof(char));
	u64 index = 0;
	u64 i = pos;

	for (; i < pos + len; i++)
		substr[index++] = str[i];

	substr[index] = '\0';
	return substr;
}

/* Returns the total number of items in the list */
u64 get_total_list_items(const struct list_item *head)
{
	u64 num = 0;

	for (; head; head = head->next)
		num++;

	return num;
}

/*
 * Returns 'true' if an item of type 'char*' is present in the provided list,
 * 'false' otherwise.
 */
bool is_present_in_list(const struct list_item *head, const char *str)
{
	for (; head; head = head->next) {
		if (!strcmp((char*)head->val, str))
			return true;
	}

	return false;
}

/*
 * Convert a list of member type 'struct list_item' into an array of
 * character pointers to enable random access via indexes.
 */
char** list_to_numbered_array(struct list_item *item)
{
	u64 num, i, len;
	char **arr;

	if (!item)
		return NULL;

	num = get_total_list_items(item);
	arr = malloc(num * sizeof(char*));

	i = 0;
	for (; i < num; i++) {
		len = strlen((char*)item->val);
		arr[i] = malloc((len + 1) * sizeof(char));

		strcpy(arr[i], (char*)item->val);
		arr[i][len] = '\0';

		item = item->next;
	}

	return arr;
}

/* Returns 'true' if the given string is a number, 'false' otherwise */
bool isnum(const char *arr)
{
	u64 i = 0;

	if (!arr)
		return false;

	for (; i < strlen(arr); i++) {
		if (!isdigit(arr[i]))
			return false;
	}

	return true;
}

/* Frees the memory for a dynamically allocated list */
void free_list(struct list_item *head)
{
	struct list_item *moving = head;
	struct list_item *temp;
	void *buf;

	while (moving) {
		buf = (void*)moving->val;
		temp = moving->next;

		free(buf);
		free(moving);

		moving = temp;
	}
}

/*
 * Returns 'true' if the file name corresponds to a symlink/hardlink, and 'false'
 * otherwise.
 */
bool is_link_nabs(const char *name)
{
	struct stat st;

	if (lstat(name, &st) != 0)
		return false;

	if (!S_ISREG(st.st_mode) || (st.st_nlink > 1)) {
		fprintf(stderr, "discovered file system corruptions, exiting...\n");
		return true;
	}

	return false;
}
