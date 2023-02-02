#include <stdbool.h>

#include "pciutils/lib/pci.h"

#define MAX_LEN	1024

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
