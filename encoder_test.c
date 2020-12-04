#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>

#define IN   0
#define OUT  1

#define NONE     0
#define RISING   1
#define FALLING  2
#define BOTH     4

#define LOW  0
#define HIGH 1

#define INT9   9
#define GPIO10 10

static int GPIOExport(int pin)
{
#define BUFFER_MAX 3
        char buffer[BUFFER_MAX];
        ssize_t bytes_written;
        int fd;

        fd = open("/sys/class/gpio/export", O_WRONLY);
        if (-1 == fd) {
                fprintf(stderr, "Failed to open export for writing!\n");
                return(-1);
        }

        bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
        write(fd, buffer, bytes_written);
        close(fd);
        return(0);
}

static int GPIOUnexport(int pin)
{
        char buffer[BUFFER_MAX];
        ssize_t bytes_written;
        int fd;

        fd = open("/sys/class/gpio/unexport", O_WRONLY);
        if (-1 == fd) {
                fprintf(stderr, "Failed to open unexport for writing!\n");
                return(-1);
        }

        bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
        write(fd, buffer, bytes_written);
        close(fd);
        return(0);
}

static int GPIODirection(int pin, int dir)
{
        static const char s_directions_str[]  = "in\0out";

#define DIRECTION_MAX 35
        char path[DIRECTION_MAX];
        int fd;

        snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);
        fd = open(path, O_WRONLY);
        if (-1 == fd) {
                fprintf(stderr, "Failed to open gpio direction for writing!\n");
                return(-1);
        }

        if (-1 == write(fd, s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3)) {
                fprintf(stderr, "Failed to set direction!\n");
                return(-1);
        }

        close(fd);
        return(0);
}

static int GPIOEdge(int pin, int edge)
{
        static const char s_edge_str[][10]  = {"none", "rising", "falling", "both"};

#define DIRECTION_MAX 35
        char path[DIRECTION_MAX];
        int fd;

        snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/edge", pin);
        fd = open(path, O_WRONLY);
        if (-1 == fd)
        {
                fprintf(stderr, "Failed to open gpio EDGE for writing!\n");
                return(-1);
        }

        if (-1 == write(fd, s_edge_str[edge], strlen(s_edge_str[edge]))) {
                fprintf(stderr, "Failed to set EDGE!\n");
                return(-1);
        }

        close(fd);
        return(0);
}

static int GPIORead(int pin)
{
#define VALUE_MAX 30
        char path[VALUE_MAX];
        char value_str[3];
        int fd;

        snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
        fd = open(path, O_RDONLY);
        if (-1 == fd) {
                fprintf(stderr, "Failed to open gpio value for reading!\n");
                return(-1);
        }

        if (-1 == read(fd, value_str, 3)) {
                fprintf(stderr, "Failed to read value!\n");
                return(-1);
        }

        close(fd);

        return(atoi(value_str));
}

static int GPIOWrite(int pin, int value)
{
        static const char s_values_str[] = "01";

        char path[VALUE_MAX];
        int fd;

        snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
        fd = open(path, O_WRONLY);
        if (-1 == fd)
        {
                fprintf(stderr, "Failed to open gpio value for writing!\n");
                return(-1);
        }

        if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1))
        {
                fprintf(stderr, "Failed to write value!\n");
                return(-1);
        }

        close(fd);
        return(0);
}

int Config_GPIO(void)
{
        /*
         * Close GPIO pins frist
         */
        if (-1 == GPIOUnexport(INT9) || -1 == GPIOUnexport(GPIO10))
                return(1);

        /*
         * Enable GPIO pins
         */
        if (-1 == GPIOExport(INT9) || -1 == GPIOExport(GPIO10))
                return(2);

        /*
         * Set GPIO directions
         */
        if (-1 == GPIODirection(INT9, IN) || -1 == GPIODirection(GPIO10, IN))
                return(3);

        /*
         * Set GPIO Edge
         */
        if (-1 == GPIOEdge(INT9, RISING) || -1 == GPIOEdge(GPIO10, NONE))
                return(4);
}

int Counter = 0;
char str;

static long get_nanos(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (long)ts.tv_sec * 1000000000L + ts.tv_nsec;
}

long nanos;
long last_nanos;

void interrupt(void)
{
        struct pollfd pfd0;
        pfd0.events = POLLPRI | POLLERR;
        pfd0.fd = open("/sys/class/gpio/gpio9/value", O_RDONLY | O_NONBLOCK);

        while (1)
        {
                lseek(pfd0.fd, 0, SEEK_SET); read (pfd0.fd, str, 1);
                poll(&pfd0, 1, 1000);

                Counter++;
                if (Counter >= 500)
                {
                        Counter = 0;

                        nanos = get_nanos();
                        long diff = nanos - last_nanos;
                        double freq = 1000000000.0f / diff;
                        last_nanos = nanos;
                        printf("%ld ns, %lf Hz, %d\n", diff, freq, GPIORead(GPIO10));
                }
        }
}
/*
// Programming: Interrupt
struct pollfd pfd0; //see "man poll"
pfd0.events = POLLPRI | POLLERR; // enable events
pfd0.fd = open("/sys/class/gpio/gpio9/value", O_RDONLY | O_NONBLOCK); // open file
lseek(pfd0.fd, 0, SEEK_SET); read (pfd0.fd, str, 1); // empty the read buffer
poll(&pfd0, 1, 1000); // this will wait for the next interrupt
lseek(pfd0.fd, 0, SEEK_SET); read (pfd0.fd, str, 1); // empty the read buffer
//poll again, consider a loop...
*/

int main(int argc, char const *argv[])
{
        //Config_GPIO();
        last_nanos = get_nanos();

        interrupt();
        return 0;
}





