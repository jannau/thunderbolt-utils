#include <string.h>
#include <stdio.h>

#include "utils.h"

int strpos(char *str, char *substr, u32 pos)
{
	char strnew[strlen(str)];
	char *pos_str;

	strncpy(strnew, str + pos, strlen(str) - pos);

	pos_str = strstr(strnew, substr);

	if (pos_str)
		return pos_str - (strnew + pos);

	return -1;
}
