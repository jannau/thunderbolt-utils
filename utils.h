#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>

#define u8		uint8_t
#define u16		uint16_t
#define u32		uint32_t
#define u64		uint64_t

#define BITMASK(x, y)	((u64)(-1) >> (63 - x)) << y
#define BIT(x)		(u64)1 << x

#define MAX_LEN		1024

#define PAGE_SIZE	sysconf(_SC_PAGE_SIZE)

#define READ_FLAG	BIT(0)
#define WRITE_FLAG	BIT(1)
#define RDWR_FLAG	READ_FLAG | WRITE_FLAG

#define msleep(x)	usleep(x * 1000)

struct list_item {
	void *val;
	struct list_item *next;
};

/*
 * Mapped virtual and physical addresses.
 * Default size is coded to 'PAGE_SIZE'.
 */
struct va_phy_addr {
	void *va;
	u64 iova;
};

struct list_item* list_add(struct list_item *tail, const void *val);
int strpos(const char *str, const char *substr, u64 offset);
char* do_bash_cmd(const char *cmd);
struct list_item* do_bash_cmd_list(const char *cmd);
char* trim_white_space(char *str);
char* switch_cmd_to_root(const char *cmd);
u64 get_page_aligned_addr(u64 off);
void* get_user_mapped_read_va(int fd, u64 off, u64 size);
void* get_user_mapped_write_va(int fd, u64 off, u64 size);
void* get_user_mapped_rw_va(int fd, u64 off, u64 size);
void unmap_user_mapped_va(void *addr, u64 size);
u64 get_size_least_set(u64 bitmask);
u32 get_crc32(u32 crc, const u8 *data, u64 size);
u8 get_crc8(u8 crc, const u8 *data, u64 size);
void convert_to_be32(u32 *data, u64 len);
void be32_to_u32(u32 *data, u64 len);
