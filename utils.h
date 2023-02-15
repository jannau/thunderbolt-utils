#include <stdbool.h>
#include <unistd.h>

#include "pciutils/lib/pci.h"

#define BITMASK(x, y)	((u64)(-1) >> (64 - x)) << y
#define BIT(x)		BITMASK(x, x)
#define MAX_LEN		1024

#define PAGE_SIZE	sysconf(_SC_PAGE_SIZE)

#define READ_FLAG	BIT(0)
#define WRITE_FLAG	BIT(1)
#define RDWR_FLAG	READ_FLAG | WRITE_FLAG

struct list_item {
	void *val;
	struct list_item *next;
};

int strpos(char *str, char *substr, u32 pos);
char* do_bash_cmd(const char *cmd);
struct list_item *do_bash_cmd_list(const char *cmd);
char *trim_white_space(char *str);
char* switch_cmd_to_root(const char *cmd);
struct list_item* list_add(struct list_item *tail, const void *val);
u64 get_page_aligned_addr(const u64 off);
void* get_user_mapped_read_va(int fd, u64 off, u64 size);
void* get_user_mapped_write_va(int fd, u64 off, u64 size);
void* get_user_mapped_rw_va(int fd, u64 off, u64 size);
void unmap_user_mapped_va(void *addr, u64 size);
u64 get_size_least_set(u64 bitmask);
u32 get_crc32(u32 crc, u8 *data, u64 size);
