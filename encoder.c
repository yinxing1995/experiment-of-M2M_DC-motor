#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>

/* Control Flag */
#define DEBUG
#define ENABLE_DIRECTION_DETECT
//#define PRINT_TO_STDIO

#ifdef PRINT_TO_STDIO
#undef DEBUG
#endif


/* Definitions */

// GPIO Support
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

// FIFO
#ifndef PRINT_TO_STDIO
#define FIFO_NAME "fifomaster645"
#endif

/* Parameters */
#define MAX_INTERVAL_TO_USING_FREQ_MEASURE 5000000L
// Unit: ns; 5ms = 5000000 ns
#define MEASURE_INTERVAL 20000
// Unit: us; 20ms = 20000 us
#define REVOLUTIONS_EVERY_NANO_SEC 2000000000
// milifrequency = (int)(1000000000/diff)*1000/500;
#define MILLIHZ_FREQ_PER_COUNT 100
// Should = 1/500/MEASURE_INTERVAL(Unit: s) * 10^3
// 100 = 1/500 * 1/0.02 * 10^3

// Record samples
#define RECORD_SAMPLE


/* Global variables */
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;

int FrequencyCounter = 0;
int If_FrequencyMeasure = 0;
int RotationDirection = 1;   //  1: Clockwise, 0: Anticlockwise.

long diff = 0;   // Unit: nanosecond

int fifo_fd = 0;

#ifdef RECORD_SAMPLE
#define RECORD_NUM 500// x * MEASURE_INTERVAL(unit:ms)
int Sample_counter = RECORD_NUM;
char *Output_addr = "sample.dat";
int Output_fd = 0;
#endif

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

        if (-1 == write(fd, (&s_directions_str[IN == dir ? 0 : 3]), IN == dir ? 2 : 3)) {
                fprintf(stderr, "Failed to set direction!\n");
                return(-1);
        }

        close(fd);
        return(0);
}

static int GPIOEdge(int pin, int edge)
{
        static const char s_edge_str[][10]  = {"none", "rising", "falling", "", "both"};

#define EDGE_MAX 35
        char path[EDGE_MAX];
        int fd;

        snprintf(path, EDGE_MAX, "/sys/class/gpio/gpio%d/edge", pin);
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
/*
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
*/
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
    return 0;
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

/* FIFO - BEGIN */
#ifndef PRINT_TO_STDIO

int FIFO_init(void)
{
    // 若fifo已存在，则直接使用，否则创建它
    // If a fifo already exists, use it directly, otherwise create it
    if ((mkfifo(FIFO_NAME, 0777) < 0) && (errno != EEXIST))
    {
        printf("cannot create fifo...\n");
        exit(1);
    }

    // Open FIFO with Write Only Mode.
    fifo_fd = open(FIFO_NAME, O_WRONLY);

    // fifo_fd = open(FIFO_NAME, O_WRONLY | O_NONBLOCK);   // Open FIFO with Write Only and non block Mode.

    if (-1 == fifo_fd)
    {
        printf("open %s for read error\n", FIFO_NAME);
        exit(1);
    }
    return 0;
}

int FIFO_Transmit(const void *buff, int len)
{
    return write(fifo_fd, buff, len);
}

void FIFO_close(void)
{
    close(fifo_fd);
    unlink(FIFO_NAME);
}

#endif
/* FIFO - END */

/* Output File - BEGIN */

int Record_init(void)
{
    Output_fd = open("sample.dat", O_CREAT | O_TRUNC | O_RDWR);
    if (-1 == Output_fd)
    {
        // ERROR
        return (-1);
    }
    return 0;
}

void Record_close()
{
    close(Output_fd);
}

/* Output File - END */

/* MAIN - BEGIN */

void Task1_encoder(void)
{
    int DirectionChangeCounter = 0, PreviousValue = 1, Value = 0;
    long nanos = 0, last_nanos = 0;
    char str[10] = "";
    struct pollfd pfd0;

    pfd0.events = POLLPRI | POLLERR;   // enable events
    pfd0.fd = open("/sys/class/gpio/gpio9/value", O_RDONLY | O_NONBLOCK);   // open file

    while (1)
    {
        /* Seek and wait for the next interrupt */
        lseek(pfd0.fd, 0, SEEK_SET); read(pfd0.fd, str, 1);
        poll(&pfd0, 1, 1000);

        nanos = get_nanos();
        Value = GPIORead(GPIO10);

        pthread_mutex_lock(&mutex1);

#ifdef ENABLE_DIRECTION_DETECT
        /* Rotation Direction Detect */
        if (Value != RotationDirection && PreviousValue == Value)
            DirectionChangeCounter++;
        if (PreviousValue != Value)
            DirectionChangeCounter = 0;

        if (DirectionChangeCounter > 3)
        {
            RotationDirection = Value;
            DirectionChangeCounter = 0;
    #ifdef DEBUG
            printf("Rotation direction changed! Now is %d.\n", RotationDirection);
    #endif
        }
        PreviousValue = Value;
#endif

        FrequencyCounter++;
        diff = nanos - last_nanos;
        /*
        if (diff > MAX_INTERVAL_TO_USING_FREQ_MEASURE)
        {
            If_FrequencyMeasure = 0;
        }
        else
        {
            If_FrequencyMeasure = 1;
        }*/
        If_FrequencyMeasure = diff > MAX_INTERVAL_TO_USING_FREQ_MEASURE ? 0 : 1;
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

void Task2_encoder(void)
{
    int freq = 0; // Unit: mHz (millihertz) 10e-3 Hz
    while (1)
    {
        usleep(MEASURE_INTERVAL);
        pthread_mutex_lock(&mutex1);
        if (If_FrequencyMeasure)
        {
            freq = FrequencyCounter * MILLIHZ_FREQ_PER_COUNT;
        }
        else
        {
            freq = diff ? REVOLUTIONS_EVERY_NANO_SEC/diff : 0;
        }
        diff = 0;
        FrequencyCounter = 0;

#ifdef ENABLE_DIRECTION_DETECT
        if (RotationDirection == 0)
            freq = -freq;
#endif

        pthread_mutex_unlock(&mutex1);

#ifdef DEBUG
        printf("The frequency is %d miliHz\n", freq);
#endif
#ifndef PRINT_TO_STDIO
        char string[20] = "";
        // memset(string, 0, sizeof(string));
        // sprintf(string, "%d\0", freq);
        snprintf(string, 20, "%07d%c", freq, '\0');
        if (-1 == FIFO_Transmit(string, 8))
        {
            printf("write Failed");
        }
#else   // defined(PRINT_TO_STDIO)
        printf("%d\n", freq);
        fflush(stdout);
#endif

#ifdef RECORD_SAMPLE
        /* Record freq to file */
        if(Sample_counter)
        {
            sprintf(string, "%d\r\n", freq);
            write(Output_fd, string, strlen(string));
            Sample_counter--;
        }
#endif
    }
}

int main(void)
{
    pthread_t t1, t2;
    char *msg1 = "task 1", *msg2 = "task 2";

    /* Initialization something */
    // Config_GPIO();
#ifdef RECORD_SAMPLE
    Record_init();
#endif

#ifndef PRINT_TO_STDIO
    FIFO_init();
#endif

    if (pthread_create(&t1, NULL, (void *)Task1_encoder, (void *)msg1))
        exit(1);
    if (pthread_create(&t2, NULL, (void *)Task2_encoder, (void *)msg2))
        exit(1);

    while (1) ;

    printf("Threads finished\n");

#ifdef RECORD_SAMPLE
    Record_close();
#endif
    return 0;
}

