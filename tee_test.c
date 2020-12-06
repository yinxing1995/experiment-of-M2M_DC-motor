#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<string.h>
#include <unistd.h>
int main(void)
{
	int fd = 0, i = 0;
	char *p = "hello_world\n";
	printf("%s",p);
        fd = open("test.txt", O_CREAT | O_TRUNC | O_RDWR);
	for(i=0;i<3;i++)
	{
		write(fd, p, strlen(p));
	}
	close(fd);
	return 0;
}
