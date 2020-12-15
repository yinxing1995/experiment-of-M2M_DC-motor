#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Choose a Group */
//#define GROUP_2
#define GROUP_6

/* Control Flag */
#define DEBUG
//#define RECORD_SAMPLE
#define LOCAL_MODE
//#define INPUT_FROM_STDIO

/*if placed on remote Rig, it is recommended to use stream_processing*/
#define STREAM_PROCESSING
//#define FRAME_PROCESSING

/* Definitions */

// PID Parameters
#define FLOAT_INIT 0.0f
#define Kp 1.2f
#define Ki 0.5f
#define Kd 0.8f

#define PID_MAX 9000
#define PID_MIN -9000

#define MEASURE_INTERVAL 20000
// Unit: us; 20ms = 20000 us

#define TARGET_FREQ 2000
// Unit: miliHz

/* Global variables */
int Serial_fd = 0;
int Output_fd = 0;
int fifo_fd = 0;

/* PID parameters */
static struct PID_parameters
{
   float P, I, D;
   float kp, ki, kd;
   int error_value;
   int previous_error_value;
   int dt;
   int PID_out;
   void (*Calculate_p)();
   void (*Calculate_i)();
   void (*Calculate_d)();
   void (*Get_PID)();
}pid;

/* Perferences */
char *Serial_addr = "/dev/serial0";
#ifdef RECORD_SAMPLE
#define RECORD_NUM 1000
int Sample_counter = RECORD_NUM;
char *Output_addr = "sample.dat";
#endif

// FIFO Definition
#ifndef LOCAL_MODE
    #define FIFO_NAME "fifoslave"
#else
    #define FIFO_NAME "fifomaster"
#endif

/* Function Declaration - BEGIN */
int Serial_init(void);
void Serial_close(void);
void Serial_Transmit(const void* buff, int len);

int File_init(void);

void PID_init();
void Proportionate();
void Intergrate();
void Deriviate();
void Sum();
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
#ifndef PRINT_TO_STDIO
int FIFO_init(void)
{
    // If a fifo already exists, use it directly, otherwise create it
    if ((mkfifo(FIFO_NAME, 0777) < 0) && (errno != EEXIST))
    {
        printf("cannot create fifo...\n");
        exit(1);
    }

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
#endif
/* FIFO - END */

/* PID_INIT - BEGIN */
void PID_init()
{
    pid.D = FLOAT_INIT;
    pid.I = FLOAT_INIT;
    pid.P = FLOAT_INIT;
    pid.kp = Kp;
    pid.ki = Ki;
    pid.kd = Kd;
    pid.dt = 0;
    pid.PID_out = 0;
    pid.error_value = 0;
    pid.previous_error_value = 0;
    pid.Calculate_p = &Proportionate;
    pid.Calculate_i = &Intergrate;
    pid.Calculate_d = &Deriviate;
    pid.Get_PID = &Sum;
}

/* PID_INIT - END */

/* MAIN PID - BEGIN */

void Output(int E)
{
    //int final_value = 0, direction = 0;
    int output_value = 0;
    char str[15];
    /* Calculate output value */
#ifdef GROUP_2
    output_value = 20 * E / 1000 + (E > 0 ? 777 : 697);
#endif

#ifdef GROUP_6
    if (E > 1000 || E < -1000)
        output_value = 30 * E / 1000 + (E > 0 ? 800 : 670);
    /*
    if (E > 2000 || E < -2000)
        output_value = 35 * E / 1000 + (E > 0 ? 800 : 670);
    */
    else
        output_value = 37 * (E > 0 ? (E-1000) : (E +1000)) / 1000 + (E > 0 ? 830 : 630);
#endif
    sprintf(str, "%d 1\\", output_value);
    Serial_Transmit(str, strlen(str));

#ifdef DEBUG
    printf("output_value = %d\n", output_value);
#endif
}

void  Proportionate()
{
    pid.P = Kp * (float)pid.error_value;
}

void Intergrate()
{
    pid.I += pid.error_value;//*(*dt);
}


void Deriviate()
{
    pid.D = (pid.error_value - pid.previous_error_value);///(*dt);
    pid.previous_error_value = pid.error_value;
}

void Sum()
{
    pid.PID_out = (int)(pid.P + Ki * pid.I + Kd * pid.D);
    if (pid.PID_out > PID_MAX)
        pid.PID_out = PID_MAX;
    else if (pid.PID_out < PID_MIN)
        pid.PID_out = PID_MIN;
}

void Compute_frequency(int milifrequency)
{
    //float time_interval = (float)MEASURE_INTERVAL / 1000 / 1000;   // us to s.
    char str[10];

#ifdef RECORD_SAMPLE
    /* Record freq to file */
    if(Sample_counter)
    {
        sprintf(str, "%d\r\n", milifrequency);
        write(Output_fd, str, strlen(str));
        Sample_counter--;
    }
#endif
    pid.error_value = TARGET_FREQ - milifrequency;
    printf("error_value = %d ", pid.error_value);

    pid.Calculate_p();
    pid.Calculate_i();
    pid.Calculate_d();
    pid.Get_PID();
    /* Limit PID output to between PID_MAX and PID_MIN */


    Output(pid.PID_out);   //Serial output begins.

#ifdef DEBUG
    printf("PID = %d P = %f I = %f D = %f\n", pid.PID_out, pid.P, pid.I, pid.D);
#endif
    //compute PID;
}

/* MAIN PID - END */

/* MAIN GET_FREQUENCY - BEGIN */
#ifdef INPUT_FROM_STDIO

void Get_frequency(void)
{
    int data;

    while (1)
    {
        if (scanf("%d", &data) != 0)
        {
            // fflush(stdout);
            Compute_frequency(data);
        }
    }
}

#elif defined(STREAM_PROCESSING)

void Get_frequency(void)
{
    char r_buf = '\0';
    uint n_buf[10] = {0};
    bool Negative = false;
    int pointer = 0, Milifrequency = 0;
    while (1)
    {
        pointer = 0;
        Negative = false;
        while (1)
        {
            read(fifo_fd,&r_buf,1);
            if(r_buf == '\0')
                break;
            if(r_buf < '0' || r_buf > '9')
            {
                if(r_buf == '-')
                    Negative = true;
                continue;
            }
            else
            {
               n_buf[pointer] = (int)r_buf - 48;
               pointer++;
               if(pointer > 9)
                   goto EXIT;
            }
        }
        switch (pointer)
        {
            case 1: Milifrequency = n_buf[0];
                    break;
            case 2: Milifrequency = n_buf[0]*10 + n_buf[1];
                    break;
            case 3: Milifrequency = n_buf[0]*100 + n_buf[1]*10 + n_buf[2];
                    break;
            case 4: Milifrequency = n_buf[0]*1000 + n_buf[1]*100 + n_buf[2]*10 + n_buf[3];
                    break;
            case 5: Milifrequency = n_buf[0]*10000 + n_buf[1]*1000 + n_buf[2]*100 + n_buf[3]*10 + n_buf[4];
                    break;
            case 6: Milifrequency = n_buf[0]*100000 + n_buf[1]*10000 + n_buf[2]*1000 + n_buf[3]*100 + n_buf[4]*10 + n_buf[5];
                    break;
            case 7: Milifrequency = n_buf[0]*1000000 + n_buf[1]*100000 + n_buf[2]*10000 + n_buf[3]*1000 + n_buf[4]*100 + n_buf[5]*10 + n_buf[6];
                    break;
            default:printf("Value is too huge, please check the parameters!!!\n");
                    break;
        }
        if(Negative)Milifrequency = - Milifrequency;
        Compute_frequency(Milifrequency);
        EXIT:
	;
    }
}

#else   // #elif defined(FRAME_PROCESSING)

void Get_frequency(void)
{
    char buffer[80];
    int data = 0, r_num = 0;

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
}

#endif
/* MAIN GET_FREQUENCY - END */

int main(void)
{
    /* Initialization something */
    Serial_init();
    PID_init();

#ifdef RECORD_SAMPLE
    Output_init();
#endif

#ifndef INPUT_FROM_STDIO
    FIFO_init();
#endif

    /* Let motor stop frist */  
    Serial_Transmit("737 1\\", 7);

    /* And then start */
    usleep(20000);
    Serial_Transmit("800 1\\", 7);

    /* Main LOOP */
    Get_frequency();

    /* Close */
    Serial_close();
    Output_close();
    FIFO_close();
    return 0;
}


