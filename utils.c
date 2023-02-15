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

#define CRC32_POLY_LE			0x82f63b78

/* Thunderbolt uses 32-bit CRC */
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

	bash_result = do_bash_cmd(path);
	return strtoul(bash_result, &bash_result, 10);
}

static bool is_cpu_le(void)
{
	char *path = "lscpu | grep 'Little Endian' | wc -l";
	char *bash_result;

	bash_result = do_bash_cmd(path);
	return strtoul(bash_result, &bash_result, 10);
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

struct list_item* list_add(struct list_item *tail, const void *val)
{
	struct list_item *temp = malloc(sizeof(struct list_item));

	temp->val = val;
	temp->next = NULL;

	if (tail == NULL)
		return temp;

	tail->next = temp;

	return temp;
}

int strpos(char *str, char *substr, const u32 offset)
{
	char strnew[strlen(str)];
	char *pos_str;
	int pos;

	strncpy(strnew, str + offset, strlen(str) - offset);
	strnew[strlen(str) - offset] = '\0';

	pos_str = strstr(strnew, substr);

	if (pos_str)
		pos = pos_str - (strnew + offset);
	else
		pos = -1;

	return pos;
}

char* do_bash_cmd(const char *cmd)
{
	char *output = malloc(MAX_LEN * sizeof(char));
	FILE *file = popen(cmd, "r");

	fgets(output, MAX_LEN, file);

	pclose(file);

	return trim_white_space(output);
}

struct list_item* do_bash_cmd_list(const char *cmd)
{
	char *output = malloc(MAX_LEN * sizeof(char));
	struct list_item *head = NULL;
	FILE *file = popen(cmd, "r");

	struct list_item *tail = head;

	while (fgets(output, MAX_LEN, file) != NULL) {
		char *temp_output = malloc(MAX_LEN * sizeof(char));

		output = trim_white_space(output);

		if (tail == NULL) {
			head = list_add(tail, (void*)output);;
			tail = head;

			output = temp_output;
			continue;
		}

		tail = list_add(tail, (void*)output);

		output = temp_output;
	}

	pclose(file);

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

	strncpy(cmd_to_return, cmd_as_root, sizeof(cmd_as_root));
	cmd_to_return[sizeof(cmd_as_root)] = '\0';
	return cmd_to_return;
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

u32 get_crc32(u32 crc, u8 *data, u64 size)
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
