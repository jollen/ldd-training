#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>

int main (int argc, char *argv[])
{
  int x;
  int fd;
  unsigned char *fb;

  printf("jk2410_lcd driver tester v3\n");

  fd = open ("/dev/cdata-fb", O_RDWR);
  if (fd < 0)
    {
      printf ("Error : Can not open framebuffer device\n");
      exit (1);
    }

  fb = (unsigned char *)mmap(0, 640*480*1, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

  for (x = 0; x < 640 * 480; x++)
    {
      *fb = 0x66; fb++; /* R */
    }

  close (fd);
}
