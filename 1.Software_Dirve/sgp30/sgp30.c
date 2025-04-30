#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

#define DRIVER_NAME "sgp30"
#define SGP30_INIT_CMD 0x2003
#define SGP30_MEASURE_CMD 0x2008
#define DEV_NAME "sgp30"
#define DEV_CLASS "sgp30_class"
#define UPDATE_INTERVAL 1000 // 1秒更新间隔

struct sgp30_device {
    struct i2c_client *client;
    struct cdev cdev;
    dev_t devno;
    struct class *class;
    struct device *device;
    struct mutex lock;
    uint16_t co2;
    uint16_t tvoc;
    bool ready;
    struct timer_list update_timer;
    struct delayed_work init_work;
};

/* CRC校验函数 */
static uint8_t sgp30_crc(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;
    size_t i, bit;
    
    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (bit = 0; bit < 8; bit++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/* 发送初始化命令 */
static void sgp30_start(struct i2c_client *client)
{
    uint8_t tx_buf[2] = {SGP30_INIT_CMD >> 8, SGP30_INIT_CMD & 0xFF};
    struct i2c_msg msg = {
        .addr = client->addr,
        .flags = 0,
        .len = 2,
        .buf = tx_buf
    };

    if (i2c_transfer(client->adapter, &msg, 1) < 0)
        dev_err(&client->dev, "初始化命令失败\n");
}

/* I2C读取函数（重命名解决冲突） */
static int sgp30_i2c_read(struct i2c_client *client, uint16_t *co2, uint16_t *tvoc)
{
    int ret, i;
    uint8_t tx_buf[2] = {SGP30_MEASURE_CMD >> 8, SGP30_MEASURE_CMD & 0xFF};
    uint8_t rx_buf[6];
    struct i2c_msg msg[2] = {
        {
            .addr = client->addr,
            .flags = 0,
            .len = 2,
            .buf = tx_buf
        },
        {
            .addr = client->addr,
            .flags = I2C_M_RD,
            .len = sizeof(rx_buf),
            .buf = rx_buf
        }
    };

    /* 发送测量命令 */
    if ((ret = i2c_transfer(client->adapter, &msg[0], 1)) < 0) {
        dev_err(&client->dev, "命令发送失败: %d\n", ret);
        return ret;
    }

    msleep(12); // 等待传感器准备数据

    /* 读取数据 */
    if ((ret = i2c_transfer(client->adapter, &msg[1], 1)) < 0) {
        dev_err(&client->dev, "数据读取失败: %d\n", ret);
        return ret;
    }

    /* 校验CRC */
    for (i = 0; i < 2; i++) {
        if (sgp30_crc(&rx_buf[i*3], 2) != rx_buf[i*3 + 2]) {
            dev_err(&client->dev, "CRC错误@%d\n", i);
            return -EIO;
        }
    }

    *co2 = (rx_buf[0] << 8) | rx_buf[1];
    *tvoc = (rx_buf[3] << 8) | rx_buf[4];
    return 0;
}

/* 定时器回调函数 */
static void update_data(struct timer_list *t)
{
    struct sgp30_device *dev = from_timer(dev, t, update_timer);
    uint16_t co2, tvoc;
    int ret;

    if (!dev->ready)
        return;

    ret = sgp30_i2c_read(dev->client, &co2, &tvoc);
    if (!ret) {
        mutex_lock(&dev->lock);
        dev->co2 = co2;
        dev->tvoc = tvoc;
        mutex_unlock(&dev->lock);
    }

    mod_timer(&dev->update_timer, jiffies + msecs_to_jiffies(UPDATE_INTERVAL));
}


/* 初始化工作队列 */
static void init_work_handler(struct work_struct *work)
{
    struct sgp30_device *dev = container_of(work, struct sgp30_device, init_work.work);
    uint16_t co2, tvoc;
    int ret;

    ret = sgp30_i2c_read(dev->client, &co2, &tvoc);
    if (ret) {
        schedule_delayed_work(&dev->init_work, msecs_to_jiffies(1500));
        return;
    }

    if (tvoc == 0) {
        schedule_delayed_work(&dev->init_work, msecs_to_jiffies(1500));
        return;
    }

    dev->ready = true;
    dev_info(&dev->client->dev, "传感器就绪\n");
}

/* 文件操作函数 */
static int sgp30_open(struct inode *inode, struct file *filp)
{
    struct sgp30_device *dev = container_of(inode->i_cdev, struct sgp30_device, cdev);
    filp->private_data = dev;
    return 0;
}

static int sgp30_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t sgp30_read(struct file *filp, char __user *buf,
                         size_t count, loff_t *fpos)
{
    struct sgp30_device *dev = filp->private_data;
    char data_str[32];
    int ret, len;

    mutex_lock(&dev->lock);
    ret = sgp30_i2c_read(dev->client, &dev->co2, &dev->tvoc); // 使用重命名的函数
    mutex_unlock(&dev->lock);

    if (ret < 0)
        return ret;

    len = snprintf(data_str, sizeof(data_str), "%d %d\n", dev->co2, dev->tvoc);
    
    if (count < len)
        return -EINVAL;
    
    if (copy_to_user(buf, data_str, len))
        return -EFAULT;

    return len;
}

static const struct file_operations sgp30_fops = {
    .owner = THIS_MODULE,
    .open = sgp30_open,
    .release = sgp30_release,
    .read = sgp30_read,
};

/* 探测函数 */
static int sgp30_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct sgp30_device *dev;
    uint16_t co2, tvoc;
    int ret;

    dev = devm_kzalloc(&client->dev, sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    dev->client = client;
    mutex_init(&dev->lock);
    i2c_set_clientdata(client, dev);
    dev->ready = false;

    /* 初始化传感器 */
    sgp30_start(client);

    /* 等待传感器就绪 */
    // while (1) {
    //     ret = sgp30_i2c_read(client, &co2, &tvoc); // 使用重命名的函数
    //     if (ret) {
    //         dev_err(&client->dev, "初始化测量失败: %d\n", ret);
    //         return ret;
    //     }
    //     if (tvoc > 0)
    //         break;
    //     mdelay(1500);
    // }

    /* 注册字符设备 */
    if ((ret = alloc_chrdev_region(&dev->devno, 0, 1, DEV_NAME)) < 0) {
        dev_err(&client->dev, "设备号分配失败\n");
        return ret;
    }

    dev->class = class_create(THIS_MODULE, DEV_CLASS);
    if (IS_ERR(dev->class)) {
        ret = PTR_ERR(dev->class);
        goto chrdev_err;
    }

    cdev_init(&dev->cdev, &sgp30_fops);
    if ((ret = cdev_add(&dev->cdev, dev->devno, 1)) < 0)
        goto class_err;

    dev->device = device_create(dev->class, NULL, dev->devno, NULL, DEV_NAME);
    if (IS_ERR(dev->device)) {
        ret = PTR_ERR(dev->device);
        goto cdev_err;
    }
    /* 初始化定时器和工作队列 */
    timer_setup(&dev->update_timer, update_data, 0);
    INIT_DELAYED_WORK(&dev->init_work, init_work_handler);
    
    /* 启动初始化检测 */
    schedule_delayed_work(&dev->init_work, msecs_to_jiffies(100));

    dev_info(&client->dev, "驱动加载成功\n");
    return 0;

cdev_err:
    cdev_del(&dev->cdev);
class_err:
    class_destroy(dev->class);
chrdev_err:
    unregister_chrdev_region(dev->devno, 1);
    return ret;
}

static int sgp30_remove(struct i2c_client *client)
{
    struct sgp30_device *dev = i2c_get_clientdata(client);

    del_timer_sync(&dev->update_timer);
    cancel_delayed_work_sync(&dev->init_work);
    device_destroy(dev->class, dev->devno);
    class_destroy(dev->class);
    cdev_del(&dev->cdev);
    unregister_chrdev_region(dev->devno, 1);

    dev_info(&client->dev, "设备已卸载\n");
    return 0;
}

/* 设备树匹配表 */
static const struct of_device_id sgp30_dt_ids[] = {
    { .compatible = "sensirion,sgp30" },
    {}
};
MODULE_DEVICE_TABLE(of, sgp30_dt_ids);

static const struct i2c_device_id sgp30_id[] = {
    { DRIVER_NAME, 0 },
    {}
};
MODULE_DEVICE_TABLE(i2c, sgp30_id);

static struct i2c_driver sgp30_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = sgp30_dt_ids,
    },
    .probe = sgp30_probe,
    .remove = sgp30_remove,
    .id_table = sgp30_id,
};

module_i2c_driver(sgp30_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("SGP30传感器驱动");