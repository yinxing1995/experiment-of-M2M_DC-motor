#include<stdio.h>
#include<fcntl.h>
#include<unistd.h>
#include<string.h>
#include<termios.h>
#include<stdlib.h>

char *p;

int main(void)
{
    int fd = 0;
    p = (char *)malloc(10);
    sprintf(p, "%d %d\\", 400, 1);
    fd = open("/dev/serial0",O_RDWR|O_NOCTTY|O_NDELAY);
    write(fd, p, strlen(p));
    tcflush(fd, TCOFLUSH);
    free(p);
    close(fd);
    return 0;
}


