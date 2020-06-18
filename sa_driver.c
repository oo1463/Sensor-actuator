#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>

MODULE_LICENSE("GPL");

#define SENSOR1 17
#define LEDRED 5
#define LEDGREEN 6

#define SWITCH 16
#define SPEAKER 12

#define DEV_NAME "sa_driver_dev"

#define IOCTL_START_NUM 0x80
#define IOCTL_NUM1 IOCTL_START_NUM+1
#define IOCTL_NUM2 IOCTL_START_NUM+2


#define SIMPLE_IOCTL_NUM 'z'
#define IOCTL1 _IOWR(SIMPLE_IOCTL_NUM, IOCTL_NUM1, unsigned long *)
#define IOCTL2 _IOWR(SIMPLE_IOCTL_NUM, IOCTL_NUM2, unsigned long *)

struct sensor_queue {
    struct list_head list;
    int arrived_time;
};


static dev_t dev_num;
static struct cdev *cd_cdev;
static int irq_num;
static int irq_num_switch;
static long start_jiffies;
unsigned long time_gap;
static struct sensor_queue queue;

static int tasklet_exit;
static int is_siren_on;

spinlock_t lock;

struct tasklet_struct my_tasklet;
struct task_struct *my_kthread = NULL;
unsigned long *userpointer;


void play(int note) {

    int i;

    for(i=0; i<5; i++){
        gpio_set_value(SPEAKER,1);
        udelay(note);
        gpio_set_value(SPEAKER,0);
        udelay(note);

    }
}

static void siren(void){
    int i,j;
    for(j=0 ; j <2 ;j ++){
        for( i=500 ; i > 150 ; i--){
            play(i);
            mdelay(1);
        }

        for( i=150 ; i < 500 ; i++){
            play(i);
            mdelay(1);
        }
    }
}

static irqreturn_t sensor_isr(int irq, void* dev_id) {

    spin_lock(&lock);
    time_gap = jiffies - start_jiffies;
    spin_unlock(&lock);

    tasklet_exit = 0;
    printk("sa : after %ld second , a person detected / PIR sensor on , siren on, ledred on\n", time_gap / HZ);
    
    gpio_set_value(LEDGREEN , 0);
    gpio_set_value(LEDRED , 1);
    tasklet_schedule(&my_tasklet);

    return IRQ_HANDLED;
}

static irqreturn_t switch_isr(int irq, void* dev_id) {

    gpio_set_value(LEDGREEN , 1);
    gpio_set_value(LEDRED , 0);
    spin_lock(&lock);
    tasklet_exit = 1;
    is_siren_on = 0;
    spin_unlock(&lock);

    return IRQ_HANDLED;
}

void tasklet_func(unsigned long recv_data){

    struct sensor_queue *tmp = 0;
    
    spin_lock(&lock);

    tmp = (struct sensor_queue*)kmalloc(sizeof(struct sensor_queue), GFP_KERNEL);  //노드 할당
    tmp->arrived_time = time_gap / HZ;
    list_add(&tmp->list, &queue.list);  // queue에  노드 추가

    spin_unlock(&lock);

    disable_irq(irq_num);

    if(is_siren_on == 0){  // 사이렌이 이미 켜져 있는 경우 siren 작동 x
        spin_lock(&lock);
        is_siren_on = 1;
        spin_unlock(&lock);

        while(1){ // cpu core를 점유하며 siren을 울림
            siren();
            if(tasklet_exit == 1) {  // switch interrupt에 의해 tasklet 종료 조건 추가
                enable_irq(irq_num);
                wake_up_process(my_kthread);  // msg 전달을 위한 thread create
                return;
            }
            mdelay(1500);
        }
    }

}

int send_queue_info(void * args){ // queue에 있는 sensor data를 user에게 전달해주는 쓰레드

    int ret;
    struct sensor_queue *tmp =0;
    struct list_head *pos =0;
    struct list_head *q =0;

    while(!kthread_should_stop()){

        list_for_each_safe(pos, q ,&queue.list){ // 큐에 있는 센서의 수신 값들을 copy_to_user을 이용해 user 영역으로 전달
            tmp = list_entry(pos, struct sensor_queue, list);
            printk("tmp->arrived_time = %d\n", tmp->arrived_time);
            printk("userpointer : %p\n", userpointer);
            ret = copy_to_user(userpointer, &(tmp->arrived_time), sizeof(unsigned long));

            list_del(pos);
            kfree(tmp);

        }
        msleep(111); // 잠들며 수신값 전달 대기
    }

    return 0;
}

void activate_sensor(void){
    enable_irq(irq_num);
    enable_irq(irq_num_switch);

    gpio_set_value(LEDGREEN , 1);
    gpio_set_value(LEDRED , 0);
    tasklet_init(&my_tasklet, tasklet_func, (unsigned long)userpointer);
}

void deativate_sensor(void){
    disable_irq(irq_num);
    disable_irq(irq_num_switch);
    gpio_set_value(LEDGREEN , 0);
    gpio_set_value(LEDRED , 0);
    tasklet_kill(&my_tasklet);

}



static long ioctl(struct file *file, unsigned int cmd, unsigned long arg){

    userpointer = (unsigned long *)arg;

    switch (cmd){
        case IOCTL1: // enable sensors
            activate_sensor(); // PIR sensor activate
            start_jiffies = jiffies;
            break;

        case IOCTL2: // disable sensors
            deativate_sensor();
            break;

        default:
            return -1;
    }

    return 0;
}

static int sa_open(struct inode *inode, struct file *file){
    return 0;
}

static int sa_release(struct inode *inode, struct file *file){
    return 0;
}

struct file_operations sa_fops = {
    .unlocked_ioctl = ioctl,
    .open = sa_open,
    .release = sa_release
};


static int __init sa_driver_init(void){
    int ret;
    alloc_chrdev_region(&dev_num, 0, 1, DEV_NAME);
    cd_cdev = cdev_alloc();
    cdev_init(cd_cdev, &sa_fops);
    cdev_add(cd_cdev, dev_num, 1);
    
    spin_lock_init(&lock);


    gpio_request_one(SENSOR1, GPIOF_IN, "sensor1");
    gpio_request_one(SPEAKER, GPIOF_OUT_INIT_LOW, "speaker");
    gpio_request_one(SWITCH, GPIOF_IN, "switch");
    gpio_request_one(LEDRED, GPIOF_OUT_INIT_LOW, "ledred");
    gpio_request_one(LEDGREEN, GPIOF_OUT_INIT_LOW, "ledgreen");

    irq_num = gpio_to_irq(SENSOR1);

    ret = request_irq(irq_num, sensor_isr, IRQF_TRIGGER_RISING , "PIR_sensor", NULL);
    if(ret) {
        printk("sa : Unable to request IRQ : %d\n", ret);
        free_irq(irq_num, NULL);
    }
    else {
        disable_irq(irq_num);
    }

    irq_num_switch = gpio_to_irq(SWITCH);

    ret = request_irq(irq_num_switch, switch_isr, IRQF_TRIGGER_RISING , "switch sensor", NULL);
    if(ret) {
        printk("sa : Unable to request IRQ : %d\n", ret);
        free_irq(irq_num_switch, NULL);
    }
    else {
        disable_irq(irq_num_switch);
    }

    my_kthread = kthread_create(send_queue_info, NULL, "sending thread");

    if(IS_ERR(my_kthread)){
        my_kthread = NULL;
    }

    INIT_LIST_HEAD(&queue.list); // 리스트 init


    // gpio_set_value(SPEAKER,0);

    return 0;
}

static void __exit sa_driver_exit(void){
    struct sensor_queue *tmp =0;
    struct list_head *pos =0;
    struct list_head *q =0;

    cdev_del(cd_cdev);
    unregister_chrdev_region(dev_num, 1);

    free_irq(irq_num,NULL);
    free_irq(irq_num_switch,NULL);

    gpio_free(SENSOR1);
    gpio_set_value(SPEAKER , 0);
    gpio_free(SPEAKER);
    gpio_free(SWITCH);
    gpio_set_value(LEDRED , 0);
    gpio_free(LEDRED);
    gpio_set_value(LEDGREEN , 0);
    gpio_free(LEDGREEN);
    
    list_for_each_safe(pos, q, &queue.list){        
        tmp = list_entry(pos, struct sensor_queue, list);
        list_del(pos);
        kfree(tmp);            
    }  

    if(my_kthread){
        kthread_stop(my_kthread);
    }

}

module_init(sa_driver_init);
module_exit(sa_driver_exit);