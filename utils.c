#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "utils.h"

int strpos(char *str, char *substr, u32 offset)
{
	char strnew[strlen(str)];
	char *pos_str;
	int pos;

	strncpy(strnew, str + offset, strlen(str) - offset);

	pos_str = strstr(strnew, substr);

	if (pos_str)
		pos = pos_str - (strnew + offset);
	else
		pos = -1;

	return pos;
}

char* do_ssh_cmd(const char *cmd)
{
	char *output = (char*)malloc(MAX_LEN * sizeof(char));
	FILE *file = popen(cmd, "r");

	fgets(output, sizeof(output), file);

	pclose(file);

	return output;
}
