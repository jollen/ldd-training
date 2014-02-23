#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "cdata_ioctl.h"

int main()
{
	int fd;
	int i;

	fd = open("/dev/cdata-fb", O_RDWR);

	for (i = 0; i < 256; i++) {
		write(fd, "A", 1);
	}

	close(fd);
}
