#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Control Flag */
#define DEBUG
#define LOCAL_MODE

/* Definitions */

// PID Parameters Definition
#define Kp 3.0f
#define Ki 2.0f
#define Kd 1.0f

#define MEASURE_INTERVAL 20000
// Unit: us; 20ms = 20000 us

#define TARGET_FREQ 3000
// Unit: miliHz

/* Global variables */
int Serial_fd = 0;
int Output_fd = 0;
int fifo_fd = 0;
float P = 0.0f, I = 0.0f, D = 0.0f;

/* Perferences */
char *Serial_addr = "/dev/serial0";
char *Output_addr = "sample.dat";

// FIFO Definition
#ifndef LOCAL_MODE
    #define FIFO_NAME "fifomaster"
#else
    #define FIFO_NAME "fifomaster"
#endif

/* Function Declaration - BEGIN */
int Serial_init(void);
void Serial_close(void);
void Serial_Transmit(const void* buff, int len);

int File_init(void);
/* Function Declaration - END */



/* Serial Support - BEGIN */

int Serial_init(void)
{
    Serial_fd = open("/dev/serial0", O_RDWR | O_NOCTTY | O_NDELAY);
    if (-1 == Serial_fd)
    {
        // ERROR
        return (-1);
    }
    return 0;
}

void Serial_close(void)
{
    close(Serial_fd);
}

void Serial_Transmit(const void* buff, int len)
{
    write(Serial_fd, buff, len);
}

/* Serial Support - END */

/* Output File - BEGIN */

int Output_init(void)
{
    Output_fd = open("sample.dat", O_CREAT | O_TRUNC | O_RDWR);
    if (-1 == Output_fd)
    {
        // ERROR
        return (-1);
    }
    return 0;
}

void Output_close(void)
{
    close(Serial_fd);
}

/* Output File - END */

/* FIFO - BEGIN */

int FIFO_init(void)
{
    // 若fifo已存在，则直接使用，否则创建它
    // If a fifo already exists, use it directly, otherwise create it
    if ((mkfifo(FIFO_NAME, 0777) < 0) && (errno != EEXIST))
    {
        printf("cannot create fifo...\n");
        exit(1);
    }

    // 以阻塞型只读方式打开fifo
    // Open FIFO using block method with Read Only Mode.
    fifo_fd = open(FIFO_NAME, O_RDONLY);
    if (-1 == fifo_fd)
    {
        printf("open %s for read error\n", FIFO_NAME);
        exit(1);
    }
    return 0;
}

void FIFO_close(void)
{
    close(fifo_fd);
    unlink(FIFO_NAME);
}

/* FIFO - END */

/* MAIN PID - BEGIN */

void Output(int E)
{
    //int final_value = 0, direction = 0;
    int output_value = 0;

    if (E > 1000 || E < -1000)
    {
        E > 0 ? (output_value = 30 * E / 1000 + 800) : (output_value = 30 * E / 1000 + 670);
    }
    else if (E < 2000 || E > -2000)
    {
        E > 0 ? (output_value = 35 * E / 1000 + 800) : (output_value = 35 * E / 1000 + 670);
    }
    else
    {
        E > 0 ? (output_value = 40 * E / 1000 + 800) : (output_value = 40 * E / 1000 + 670);
    }
/*
    const int n[] = {800, 670};
    const int m[] = {30, 35, 40};
    int i = E/1000 ? ( (E < 2000 || E > -2000) ? 1 : 2) : 0;

    output_value = m[i] * E / 1000 + n[E > 0 ? 0 : 1];*/

    char str[15];
    sprintf(str, "%d 1\\", output_value);
    Serial_Transmit(str, strlen(str));
    // write(Serial_fd, p, strlen(p));
#ifdef DEBUG
    printf("output_value = %d\n", output_value);
#endif
}

void Proportional(int *x, float *y)
{
    *y = Kp * (float)(*x);
}

void Intergrate(const float* dt, int* x, float* y)
{
    *y = *y + (*x)*(*dt);
    //printf("change_y = %f\n", (*x)*(*dt));
}

void Compute_frequency(int milifrequency)
{
    int error_value = 0, intergrate_value = 0, differentiate_value = 0;
    float time_interval = (float)MEASURE_INTERVAL / 1000 / 1000;   // us to s.
    int PID = 0;
    char str[10];
    sprintf(str, "%d\r\n", milifrequency);
    write(Output_fd, str, strlen(str));
    error_value = TARGET_FREQ - milifrequency;
    //printf("error_value = %d ", error_value);
    Proportional(&error_value, &P);
    Intergrate(&time_interval, &error_value, &I);
    PID = (int)(P + Ki*I);

    Output(PID);

#ifdef DEBUG
    printf("PID = %d P = %f I = %f\n", PID, P, I);
#endif
    //compute PID;
}

/* MAIN PID - END */

int main(int argc, char const *argv[])
{
    char buffer[80];
    int data = 0, r_num = 0;

    /* Initialization something */
    Serial_init();
    Output_init();
    FIFO_init();

    /* Let motor stop frist */
    Serial_Transmit("737 1\\", 7);
    /* And then start */
    usleep(20000);
    Serial_Transmit("800 1\\", 7);

    while (1)
    {
        while ((r_num = read(fifo_fd, buffer, sizeof(buffer))) == 0) ;
        if (sscanf(buffer, "%d", &data) != 0)
        {
            Compute_frequency(data);
#ifdef DEBUG
            printf("%d bytes received, data is: %d, raw: %s\n", r_num, data, buffer);
#endif
        }
#ifdef DEBUG
        else
            printf("%d bytes received, raw: %s\n", r_num, buffer);
#endif
    }

    Serial_close();
    Output_close();
    FIFO_close();
    return 0;
}
