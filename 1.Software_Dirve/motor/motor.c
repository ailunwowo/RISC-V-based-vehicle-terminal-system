#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#define DRIVER_NAME "step_motor_driver"
#define DEVICE_NAME "step_motor"

// 定义GPIO引脚
#define GPIO_MOTOR_A 54
#define GPIO_MOTOR_B 51
#define GPIO_MOTOR_C 56
#define GPIO_MOTOR_D 40

// IOCTL命令定义
#define CMD_STEPMOTOR_A _IOW('L', 0, unsigned long)
#define CMD_STEPMOTOR_B _IOW('L', 1, unsigned long)
#define CMD_STEPMOTOR_C _IOW('L', 2, unsigned long)
#define CMD_STEPMOTOR_D _IOW('L', 3, unsigned long)

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("wdb");

static long step_motor_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    switch(cmd) {
        case CMD_STEPMOTOR_A:
            gpio_set_value(GPIO_MOTOR_A, arg);
            break;
        case CMD_STEPMOTOR_B:
            gpio_set_value(GPIO_MOTOR_B, arg);
            break;
        case CMD_STEPMOTOR_C:
            gpio_set_value(GPIO_MOTOR_C, arg);
            break;
        case CMD_STEPMOTOR_D:
            gpio_set_value(GPIO_MOTOR_D, arg);
            break;
        default:
            return -ENOTTY;
    }
    return 0;
}

static int step_motor_open(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "Step motor device opened\n");
    return 0;
}

static int step_motor_release(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "Step motor device closed\n");
    return 0;
}

static struct file_operations step_motor_fops = {
    .owner = THIS_MODULE,
    .open = step_motor_open,
    .release = step_motor_release,
    .unlocked_ioctl = step_motor_ioctl,
};

static struct miscdevice step_motor_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DEVICE_NAME,
    .fops = &step_motor_fops,
};

static int __init step_motor_init(void)
{
    int ret;
    
    printk(KERN_INFO "Initializing step motor driver\n");
    
    // 申请GPIO资源
    if ((ret = gpio_request(GPIO_MOTOR_A, "motor-gpio-a")) < 0) {
        printk(KERN_ERR "Failed to request GPIO %d: %d\n", GPIO_MOTOR_A, ret);
        return ret;
    }
    
    if ((ret = gpio_request(GPIO_MOTOR_B, "motor-gpio-b")) < 0) {
        printk(KERN_ERR "Failed to request GPIO %d: %d\n", GPIO_MOTOR_B, ret);
        gpio_free(GPIO_MOTOR_A);
        return ret;
    }
    
    if ((ret = gpio_request(GPIO_MOTOR_C, "motor-gpio-c")) < 0) {
        printk(KERN_ERR "Failed to request GPIO %d: %d\n", GPIO_MOTOR_C, ret);
        gpio_free(GPIO_MOTOR_A);
        gpio_free(GPIO_MOTOR_B);
        return ret;
    }
    
    if ((ret = gpio_request(GPIO_MOTOR_D, "motor-gpio-d")) < 0) {
        printk(KERN_ERR "Failed to request GPIO %d: %d\n", GPIO_MOTOR_D, ret);
        gpio_free(GPIO_MOTOR_A);
        gpio_free(GPIO_MOTOR_B);
        gpio_free(GPIO_MOTOR_C);
        return ret;
    }
    
    // 设置为输出模式，初始状态为低电平
    gpio_direction_output(GPIO_MOTOR_A, 0);
    gpio_direction_output(GPIO_MOTOR_B, 0);
    gpio_direction_output(GPIO_MOTOR_C, 0);
    gpio_direction_output(GPIO_MOTOR_D, 0);
    
    // 注册misc设备
    ret = misc_register(&step_motor_dev);
    if (ret) {
        printk(KERN_ERR "Failed to register misc device: %d\n", ret);
        gpio_free(GPIO_MOTOR_A);
        gpio_free(GPIO_MOTOR_B);
        gpio_free(GPIO_MOTOR_C);
        gpio_free(GPIO_MOTOR_D);
        return ret;
    }
    
    printk(KERN_INFO "Step motor driver initialized\n");
    return 0;
}

static void __exit step_motor_exit(void)
{
    // 释放GPIO资源
    gpio_free(GPIO_MOTOR_A);
    gpio_free(GPIO_MOTOR_B);
    gpio_free(GPIO_MOTOR_C);
    gpio_free(GPIO_MOTOR_D);
    
    // 注销设备
    misc_deregister(&step_motor_dev);
    
    printk(KERN_INFO "Step motor driver removed\n");
}

module_init(step_motor_init);
module_exit(step_motor_exit);