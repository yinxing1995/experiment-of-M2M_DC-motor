//写进程
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>

#define FIFO_NAME "fifomaster"
int main(void)
{
    int fd;
    char w_buf[50];
    int w_num;
 
    if((mkfifo(FIFO_NAME,0777)<0)&&(errno!=EEXIST))
    {
        printf("cannot create fifo...\n");
        exit(1);
    }
    fd=open(FIFO_NAME,O_WRONLY);
    while(1)
    {
    	w_num=write(fd,"abcdg\0",6);
	usleep(200000);
    	printf("%d\n",w_num);
    }
    return 0;
}
