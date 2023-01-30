#include "pciutils/lib/pci.h"

#define MAX_LEN	1024

struct list_item {
	char *val;
	struct list_item *next;
};

int strpos(char *str, char *substr, u32 pos);
char* do_bash_cmd(const char *cmd);
struct list_item *do_bash_cmd_list(const char *cmd);
char *trim_white_space(char *str);
char* switch_cmd_to_root(const char *cmd);
