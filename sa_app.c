#include "sa_app.h"
#include <unistd.h>


int main(void){
    int input;

    dev = open("/dev/sa_driver_dev", O_RDWR);

    while(1){
        printf("Sensor activate : 1\n");
        printf("Sensor deactivate : 2\n");
        printf("Application exit : 3\n");

        
        scanf("%d", &input);

        user_buf = 0;
        // printf("pointer = %p\n",&user_buf);  

        switch(input){
            case 1:
                activate_sensor(&user_buf);
                break;
            case 2:
                deactivate_sensor(&user_buf);
                
                break;
            
            case 3:
                close(dev);
                return 0;

            default:
                break;
        }
    }


    close(dev);

    return 0;
}