#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <pthread.h>

#define IOCTL_START_NUM 0x80
#define IOCTL_NUM1 IOCTL_START_NUM+1
#define IOCTL_NUM2 IOCTL_START_NUM+2


#define SIMPLE_IOCTL_NUM 'z'
#define IOCTL1 _IOWR(SIMPLE_IOCTL_NUM, IOCTL_NUM1, unsigned long *)
#define IOCTL2 _IOWR(SIMPLE_IOCTL_NUM, IOCTL_NUM2, unsigned long *)



int dev;
unsigned long user_buf;
pthread_t msgcheck_thread;

void *rcv_msg_thread(void *args){
    user_buf = 0;
    while(1){
        if(user_buf){
            printf("sensor detected after %ld seconds\n\n ", user_buf);
            user_buf = 0;
        }
        // printf("checking thread : user_buf = %ld // pointer : %p\n",user_buf , &user_buf);
        usleep(99000);
    }
}


// int checking_msg(void){
    
//     int status;
//     status = pthread_create(&msgcheck_thread, NULL, &rcv_msg_thread, NULL);

//     return status;
// }

int activate_sensor(unsigned long *user_buff){
    int ret;
    printf("Sensor activated\n");
    ret = ioctl(dev, IOCTL1, user_buff);
    pthread_create(&msgcheck_thread, NULL, &rcv_msg_thread, NULL);

    return ret;
}

int deactivate_sensor(unsigned long *user_buff){
    int ret;
    printf("Sensor deactivated\n");

    ret = ioctl(dev, IOCTL2, user_buff);
    
    pthread_cancel(msgcheck_thread);
    return ret;
}


