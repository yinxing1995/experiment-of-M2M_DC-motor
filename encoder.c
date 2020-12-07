#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <time.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>

char str = '\0';
int FrequencyCounter = 0;
int If_FrequencyMeasure = 0;
long nanos = 0;
long last_nanos = 0;
long diff = 0;
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;

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

#define MAX_INTERVAL_TO_USING_FREQ_MEASURE 5000000L
// Unit: ns; 5ms = 5000000 ns
#define REVOLUTIONS_EVERY_NANO_SEC 2000000000
// milifrequency = (int)(1000000000/diff)*1000/500;
#define MEASURE_INTERVAL 20000
// Unit: us; 20ms = 20000 us
#define TARGET_FREQ 3000
// Unit: miliHz


/* GPIO Support - BEGIN */

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
        write(fd, (const void*)buffer, bytes_written);
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
        write(fd, (const void*)buffer, bytes_written);
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

        if (-1 == write(fd, (const void*)s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3)) {
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

        if (-1 == write(fd, (const void*)s_edge_str[edge], strlen(s_edge_str[edge]))) {
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

        if (-1 == read(fd, (void *)value_str, 3)) {
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

        if (1 != write(fd, (const void*)&s_values_str[LOW == value ? 0 : 1], 1))
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

/* GPIO Support - END */

/* Timer Support - BEGIN */

static long get_nanos(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (long)ts.tv_sec * 1000000000L + ts.tv_nsec;
}

/* Timer Support - END */

/* Main - BEGIN */

void Task1_encoder()
{
    //last_nanos = get_nanos();
    struct pollfd pfd0;
    pfd0.events = POLLPRI | POLLERR;
    pfd0.fd = open("/sys/class/gpio/gpio9/value", O_RDONLY | O_NONBLOCK);
    while (1)
    {
        lseek(pfd0.fd, 0, SEEK_SET); read (pfd0.fd, (void *)str, 1);
        poll(&pfd0, 1, 1000);
        nanos = get_nanos();
        pthread_mutex_lock(&mutex1);
        FrequencyCounter++;
        diff = nanos - last_nanos;
        if (diff > MAX_INTERVAL_TO_USING_FREQ_MEASURE)
        {
            If_FrequencyMeasure = 0;
        }
        else
        {
            If_FrequencyMeasure = 1;
        }
        pthread_mutex_unlock(&mutex1);
        last_nanos = nanos;
        /*
        if (Counter >= 500)
        {
            Counter = 0;
            nanos = get_nanos();
            long diff = nanos - last_nanos;
            double freq = 1000000000.0f / diff;
            pthread_mutex_lock(&mutex1);
            frequency = freq*1000;
            pthread_mutex_unlock(&mutex1);
            last_nanos = nanos;
            printf("%ld ns, %lf Hz, %d\n", diff, freq, GPIORead(GPIO10));
        }
        */
    }
}

void Task2_encoder()
{
    int freq = 0; // Unit: mHz (millihertz) 10e-3 Hz
    while (1)
    {
        usleep(MEASURE_INTERVAL); 
        pthread_mutex_lock(&mutex1);
        if (If_FrequencyMeasure)
        {
            freq = FrequencyCounter * 100;
        }
        else
        {
            freq = diff ? REVOLUTIONS_EVERY_NANO_SEC/diff : 0;
        }
        diff = 0;
        FrequencyCounter = 0;
        pthread_mutex_unlock(&mutex1);
        printf("The frequency is %d miliHz\n", freq);
        // Compute_frequency(freq);
    }
}

int main(void)
{
    pthread_t t1,t2;
    char *msg1 = "task 1", *msg2 = "task 2";
    if (pthread_create(&t1, NULL, (void *)Task1_encoder, (void *)msg1))
        exit(1);
    if (pthread_create(&t2, NULL, (void *)Task2_encoder, (void *)msg2))
        exit(1);
    while (1) ;
    printf("Threads finished\n");
    return 0;
}
