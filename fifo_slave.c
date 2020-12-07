//读进程
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define FIFO_NAME "fifoslave"

int main(void)
{
    char r_buf[50];
    int  fd;
    int  r_num;

// 若fifo已存在，则直接使用，否则创建它
    if((mkfifo(FIFO_NAME,0777)<0)&&(errno!=EEXIST))
    {
        printf("cannot create fifo...\n");
        exit(1);
    }
    //以阻塞型只读方式打开fifo
    fd=open(FIFO_NAME,O_RDONLY);
    if(fd==-1)
    {
	printf("open %s for read error\n");
	exit(1);
    }
    // 通过键盘输入字符串，再将其写入fifo，直到输入"exit"为止
    while(1)
    {
	usleep(15000);
    	r_num=read(fd,r_buf,6);
    	printf(" %d bytes read:%s\n",r_num,r_buf);
    }
    unlink(FIFO_NAME);//删除fifo
    return 0;
}

