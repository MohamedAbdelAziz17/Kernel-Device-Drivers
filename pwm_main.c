#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/pwm.h>
#include <linux/kernel.h>

/*Meta Information*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("MohamedGalal");
MODULE_DESCRIPTION("A hello world Psuedo device driver");

/*Prototypes*/
static int driver_open (struct inode *device_file, struct file *instance);
static int driver_close (struct inode *device_file, struct file *instance);
ssize_t driver_read (struct file * file, char __user *user_buffer , size_t count, loff_t *off);
ssize_t driver_write (struct file * file, const char __user * user_buffer, size_t count, loff_t * off);


/*Declarations*/
#define LED_PIN 2
struct class* myClass;
struct cdev st_character_device;
struct file_operations fops =
{
    .owner = THIS_MODULE,
    .open  = driver_open,
    .release = driver_close,
    .read  = driver_read,
    .write = driver_write
};
dev_t device_number;
struct device* mydevice;
#define BUFF_SIZE 15
static unsigned char buffer[BUFF_SIZE] = ""; // this at kernalspace

/***************PWM***************/
#define duty_cycle0 75  //percent
#define period0     500 //ms
#define PWM0_CREATE 1
u32 pwm_high0 = 0; //500Mhz
u32 pwm_period0 =  0; // 1ns
struct pwm_device *pwm0_dev = NULL;

#define duty_cycle1 25  //percent
#define period1     500 //ms
#define PWM1_CREATE 1
u32 pwm_high1 = 0;
u32 pwm_period1 =  0;
struct pwm_device *pwm1_dev = NULL;

int pwm0_isCreated = 0;
int pwm1_isCreated = 0;

/*Implementations*/                                 // this at userspace
ssize_t driver_read (struct file * file, char __user *user_buffer , size_t count, loff_t *off)
{
    int not_copied;

    printk("%s: the count to read %d \n",__func__, count);
    printk("%s: the offset %lld \n",__func__, *off);
    if(count + *off > BUFF_SIZE){
        count = BUFF_SIZE - *off;
    }
    not_copied  = copy_to_user(user_buffer,&buffer[*off],count);
    if(not_copied){
        return -1;
    }
    *off = count;
    printk("%s: not copied %d \n",__func__, not_copied);
    printk("%s: message --> %s \n", __func__ , user_buffer);
    return count;
}

ssize_t driver_write (struct file * file, const char __user * user_buffer, size_t count, loff_t * off)
{
    int not_copied;
    unsigned long user_dutycycle;
    
    printk("%s: written size: %d \n",__func__, count);
    if(count + *off > BUFF_SIZE){
        count = BUFF_SIZE - *off;
    }
    if(count == 0){
        printk("no space left\n");
        return -1;
    }
    not_copied  = copy_from_user(&buffer[*off], user_buffer, count);
    if(not_copied){
        return -1;
    }
    *off = count;

    //convert string to int
    if(kstrtoul(buffer,10,&user_dutycycle) != 0) return -1;
    
    //from 0 to 1000 ms
    if(user_dutycycle > 1000)
        printk("Invalid dutycycle value\n");
    else
        pwm_config(pwm0_dev,user_dutycycle * 1000000,1000000000);

    printk("%s: message --> %s \n", __func__ , buffer);
    return count;
}
 

/**
 * @brief This function is called, when the module is loaded into the kernel
 */

static int __init hellodrive_init(void)
{
    int retval;

    //1- allocate device number
    retval = alloc_chrdev_region(&device_number,0,1,"pwm_device");
    if(retval == 0)
    {
        printk("%s retval=0 registered! - major: %d Minor: %d\n",__FUNCTION__,MAJOR(device_number),MINOR(device_number));
    }
    else{
        printk("could not register device number\n");
        return -1;
    }

    //2- add a char device to the system
    cdev_init(&st_character_device, &fops);
    retval = cdev_add(&st_character_device , device_number , 1);
    if(retval != 0){
        printk("Registering device is failed!\n");
        goto CHARACTER_ERROR;
    }
    printk("cdev_init passed - cdev_add passed\n");

    //3- generate file ( class - device )
    myClass = class_create(THIS_MODULE,"pwm_class");
    if(myClass == NULL){
        printk("Device class failed!\n");
        goto CLASS_ERROR;
    }
    printk("class_create passed\n");

    //4- create the device file
    mydevice = device_create(myClass,NULL,device_number,NULL,"myPWM");
    if(mydevice == NULL){
        printk("Device create failed!\n");
        goto DEVICE_ERROR;
    }
    printk("device_create passed\n");

    //PWM0 Configuration
#if PWM0_CREATE == 1
    pwm0_dev = pwm_request(0,"default");
    if(pwm0_dev == NULL || IS_ERR(pwm0_dev))
    {
        printk("pwm0 device creation failed!, pwm0_dev: %d\n",(int)pwm0_dev);
        goto PWM_DEV_ERROR;
    }
    //create pwm0
    pwm_period0 = period0 * 1000000;
    pwm_high0 = ( (float)duty_cycle0/100) * pwm_period0;
    printk("pwm_period: %d\npwm_high: %d\n",pwm_period0,pwm_high0);
    pwm_config(pwm0_dev, pwm_high0 , pwm_period0);
    pwm_enable(pwm0_dev);
    pwm0_isCreated = 1;
#endif

#if PWM1_CREATE == 1
    pwm1_dev = pwm_request(1,"default");
    if(pwm1_dev == NULL || IS_ERR(pwm1_dev))
    {
        printk("pwm1 device creation failed!, pwm1_dev: %d\n",(int)pwm1_dev);
        goto PWM_DEV_ERROR;
    }  
    //create pwm1
    pwm_period1 = period1 * 1000000;
    pwm_high1 = ( (float)duty_cycle1/100) * pwm_period1;
    printk("pwm_period: %d\npwm_high: %d\n",pwm_period1,pwm_high1);
    pwm_config(pwm1_dev, pwm_high1 , pwm_period1);
    pwm_enable(pwm1_dev);
    pwm1_isCreated = 1;
#endif

    return 0;

    //ERROR Hanlding
    PWM_DEV_ERROR:
        device_destroy(myClass,device_number);
    DEVICE_ERROR:
        cdev_del(&st_character_device);
    CLASS_ERROR:
        class_destroy(myClass);
    CHARACTER_ERROR:
        unregister_chrdev_region(device_number,1);
    return -1;
}
/**
 * @brief This function is called, when the module is removed from the kernel
 */
static void __exit hellodrive_exit(void)
{
    if(pwm0_isCreated == 1)
    {
        pwm_disable(pwm0_dev);
        pwm_free(pwm0_dev);
        printk("pwm0 device freed\n");
    }
    if(pwm1_isCreated == 1)
    {
        pwm_disable(pwm1_dev);
        pwm_free(pwm1_dev);
        printk("pwm1 device freed\n");
    }
    cdev_del(&st_character_device);  //delete device number
    device_destroy(myClass,device_number); //delete device
    class_destroy(myClass); //delete class
    unregister_chrdev_region(device_number,1); //unregister
    printk("Module exit!\n");
}

static int driver_open(struct inode *device_file, struct file *instance)
{
    printk("%s dev_nr - open was called!\n",__FUNCTION__);
    return 0;
}
static int driver_close(struct inode *device_file, struct file *instance)
{
    printk("%s dev_nr - close was called!\n",__FUNCTION__);
    return 0;
}

// Acts as constructor of the module
module_init(hellodrive_init);
// Acts as deconstructor of the module
module_exit(hellodrive_exit);