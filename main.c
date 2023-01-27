#include <stdio.h>

#include "pciutils.h"

int main()
{
	char *s = trim_host_pci_id(0);

	printf("present:%s", s);
}
