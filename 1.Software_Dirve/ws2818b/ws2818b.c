#include <linux/cdev.h>
#include <linux/device.h> //class devise声明
#include <linux/fs.h>	  //file_operations声明
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>   //设备号 dev_t 类型声明
#include <linux/uaccess.h> //copy_from_user 的头文件
#define gpio_num 50
static int major;
static dev_t led_dev;
static struct class *led_class;
static struct device *led_device;
static struct cdev led_cdev;
static int led_io_open(struct inode *inode, struct file *filp) {
	printk("led_open\n");
	gpio_request(gpio_num, NULL);
	return 0;
}
static ssize_t led_io_write(struct file *filp, const char __user *buf,
			    size_t count, loff_t *fpos) {
	int Cmd = 0; //上层是整数1
	printk("io_write\n");
	copy_from_user(&Cmd, buf, count);
	if (Cmd == 1) {
		printk("set 1\n");
		gpio_direction_output(gpio_num, 1);
	} else if (Cmd == 0) {
		printk("set 0\n");
		gpio_direction_output(gpio_num, 0);
	} else {
		printk("cmd error\n");
	}
	return count;
}
static ssize_t led_io_read(struct file *filp, char __user *buf, size_t count,
			   loff_t *fpos) {
	printk("ledread!!");
	unsigned long err;
	char *ptr = (char)__get_free_page(GFP_KERNEL);
	if (!ptr) {
		return -ENOMEM;
	}

	if (copy_to_user(buf, ptr, count)) {
		free_page((unsigned long)ptr);
		return -EFAULT;
	}
	free_page((unsigned long)ptr);
	return count;
}
static const struct file_operations led_io_fops = {
    .owner = THIS_MODULE,
    .open = led_io_open,
    .read = led_io_read,
    .write = led_io_write,
};
static int __init led_module_init(void) {
	printk("Hello, module is installed !\n");
	int ret;
	// 分配设备号
	if (alloc_chrdev_region(&led_dev, 0, 1, "led_device") < 0) {
		printk(KERN_ERR "Failed to allocate device number\n");
		return -1;
	}
	// 创建设备类
	led_class = class_create(THIS_MODULE, "led_class");
	if (IS_ERR(led_class)) {
		printk(KERN_ERR "Failed to create class\n");
		unregister_chrdev_region(led_dev, 1);
		return -1;
	}
	// 创建设备节点
	led_device = device_create(led_class, NULL, led_dev, NULL, "ws2818b");
	if (IS_ERR(led_device)) {
		printk(KERN_ERR "Failed to create device\n");
		class_destroy(led_class);
		unregister_chrdev_region(led_dev, 1);
		return -1;
	}
	// 注册字符设备驱动
	cdev_init(&led_cdev, &led_io_fops);
	ret = cdev_add(&led_cdev, led_dev, 1);
	if (ret < 0) {
		printk(KERN_ERR "Failed to add device to kernel\n");
		device_destroy(led_class, led_dev);
		class_destroy(led_class);
		unregister_chrdev_region(led_dev, 1);
		return -1;
	}
	return 0;
}
static void __exit led_module_exit(void) {
	gpio_free(gpio_num);
	printk("Good-bye, module was removed!\n");
	// 删除字符设备驱动并从内核中删除设备节点
	cdev_del(&led_cdev);
	device_destroy(led_class, led_dev);
	// 销毁设备类
	class_destroy(led_class);
	// 释放设备号资源
	unregister_chrdev_region(led_dev, 1);
}
module_init(led_module_init);
module_exit(led_module_exit);
MODULE_LICENSE("GPL");