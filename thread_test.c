#include<stdio.h>
#include<pthread.h>
#include<stdlib.h>
#include<unistd.h>

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;

void Task1_encoder()
{
    while(1)
    {
        usleep(1000000);
	pthread_mutex_lock(&mutex1);
        printf("I am task1\n");
	pthread_mutex_unlock(&mutex1);
    }
}

void Task2_encoder()
{
    while(1)
    {
	usleep(200000);
        pthread_mutex_lock(&mutex1);	
        usleep(1800000);
        printf("I am task2\n");
        pthread_mutex_unlock(&mutex1);
    }
}

int main(void)
{
    pthread_t t1,t2;
    char *msg1 = "task1", *msg2 = "task 2";
    if(pthread_create(&t1, NULL, (void *)Task1_encoder, (void *)msg1))
    exit(1);
    if(pthread_create(&t2, NULL, (void *)Task2_encoder, (void *)msg2))
    exit(1);
    while(1);
    printf("Threads finished\n");
}

