#include<stdio.h>
#include<fcntl.h>
#include<unistd.h>
#include<string.h>
#include <termios.h>

char *p = "echo 900 1\\";

int main(void)
{
    int fd = 0;
    fd = open("/dev/serial0",O_RDWR|O_NOCTTY|O_NDELAY);
    write(fd, p, strlen(p));
    usleep(10000);
   //tcflush(fd, TCOFLUSH);
    close(fd);
    return 0;
}

