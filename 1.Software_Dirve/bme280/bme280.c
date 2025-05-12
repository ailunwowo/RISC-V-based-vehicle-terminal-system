#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#define BME280_ADDR        0xEC
#define BME280_CHIP_ID_REG 0xD0
#define BME280_CTRL_HUM    0xF2
#define BME280_CTRL_MEAS   0xF4
#define BME280_CONFIG      0xF5
#define BME280_PRESS_MSB   0xF7
#define SCL_GPIO           36
#define SDA_GPIO           61

/* 校准参数结构体 */
struct bme280_calib {
    u16 dig_T1;
    s16 dig_T2, dig_T3;
    u16 dig_P1;
    s16 dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
    u8  dig_H1;
    s16 dig_H2;
    u8  dig_H3;
    s16 dig_H4, dig_H5;
    u8  dig_H6;
    s32 t_fine;
};

/* 设备数据结构 */
struct bme280_dev {
    struct mutex lock;
    struct bme280_calib calib;
    bool calib_valid; 
};

static int major;
static dev_t i2c_dev;
static struct class *i2c_class;
static struct device *i2c_device;
static struct cdev i2c_cdev;
static struct bme280_dev dev;

/* GPIO操作 */
#define SDA_HIGH()  gpio_set_value(SDA_GPIO, 1)
#define SDA_LOW()   gpio_set_value(SDA_GPIO, 0)
#define SCL_HIGH()  gpio_set_value(SCL_GPIO, 1)
#define SCL_LOW()   gpio_set_value(SCL_GPIO, 0)
#define SDA_READ()  gpio_get_value(SDA_GPIO)


static void i2c_delay(void) {
    udelay(5);
}
static void i2c_stop(void) {
    SDA_LOW();
    SCL_HIGH();
    i2c_delay();
    SDA_HIGH();
    i2c_delay();
}
/* 总线恢复函数 */
static void i2c_recover(void) {
    int i;
    gpio_direction_output(SCL_GPIO, 1);
    for (i = 0; i < 9; i++) {
        SCL_HIGH();
        i2c_delay();
        SCL_LOW();
        i2c_delay();
    }
    i2c_stop();
}

static void i2c_start(void) {
    SDA_HIGH();
    SCL_HIGH();
    i2c_delay();
    SDA_LOW();
    i2c_delay();
    SCL_LOW();
}

static u8 i2c_write_byte(u8 data) {
    int i;
    u8 ack;
    for (i = 7; i >= 0; i--) {
        SCL_LOW();
        (data & (1 << i)) ? SDA_HIGH() : SDA_LOW();
        i2c_delay();
        SCL_HIGH();
        i2c_delay();
    }
    SCL_LOW();
    SDA_HIGH(); 
    i2c_delay();
    SCL_HIGH();
    i2c_delay();
    ack = SDA_READ();
    SCL_LOW();
    return ack;
}

static u8 i2c_read_byte(u8 ack) {
    int i;
    u8 data = 0;
    SDA_HIGH(); 
    gpio_direction_input(SDA_GPIO);
    for (i = 7; i >= 0; i--) {
        SCL_LOW();
        i2c_delay();
        SCL_HIGH();
        data |= (SDA_READ() << i);
        i2c_delay();
    }
    SCL_LOW();
    gpio_direction_output(SDA_GPIO, 1);
    ack ? SDA_LOW() : SDA_HIGH();
    i2c_delay();
    SCL_HIGH();
    i2c_delay();
    SCL_LOW();
    SDA_HIGH();
    return data;
}

/* 多字节读写 */
static int i2c_write_reg(u8 reg, u8 value) {
    int ret = -EIO;
    mutex_lock(&dev.lock);
    
    i2c_start();
    if (i2c_write_byte(BME280_ADDR & 0xFE)) 
        goto exit;
    if (i2c_write_byte(reg)) 
        goto exit;
    if (i2c_write_byte(value)) 
        goto exit;
    ret = 0;

exit:
    i2c_stop();
    if (ret) {
        printk(KERN_ERR "BME280写寄存器失败 0x%02X\n", reg);
        i2c_recover();
    }
    mutex_unlock(&dev.lock);
    return ret;
}

static int i2c_read_regs(u8 reg, u8 *buf, int len) {
    int i, ret = -EIO;
    mutex_lock(&dev.lock);
    
    i2c_start();
    if (i2c_write_byte(BME280_ADDR & 0xFE))
        goto exit;
    if (i2c_write_byte(reg)) 
        goto exit;
    i2c_start();
    if (i2c_write_byte(BME280_ADDR | 0x01)) 
        goto exit;
    
    for (i = 0; i < len; i++)
        buf[i] = i2c_read_byte(i == len-1 ? 0 : 1);
    ret = len;

exit:
    i2c_stop();
    if (ret < 0) {
        printk(KERN_ERR "BME280读寄存器失败 0x%02X\n", reg);
        i2c_recover();
    }
    mutex_unlock(&dev.lock);
    return ret;
}

/* 校准参数解析 */
static int bme280_read_calib(void) {
    u8 data[33] = {0};
    int ret;
    
    dev.calib_valid = false;
    
    // 读取温度压力校准参数
    ret = i2c_read_regs(0x88, data, 24);
    if (ret != 24)
        return -EIO;
    
    // 读取H1
    ret = i2c_read_regs(0xA1, &data[24], 1);
    if (ret != 1)
        return -EIO;
    
    // 读取H2-H6
    ret = i2c_read_regs(0xE1, &data[25], 7);
    if (ret != 7)
        return -EIO;

    // 解析参数
    dev.calib.dig_T1 = (data[1] << 8) | data[0];
    dev.calib.dig_T2 = (s16)((data[3] << 8) | data[2]);
    dev.calib.dig_T3 = (s16)((data[5] << 8) | data[4]);
    
    dev.calib.dig_P1 = (data[7] << 8) | data[6];
    dev.calib.dig_P2 = (s16)((data[9] << 8) | data[8]);
    dev.calib.dig_P3 = (s16)((data[11] << 8) | data[10]);
    dev.calib.dig_P4 = (s16)((data[13] << 8) | data[12]);
    dev.calib.dig_P5 = (s16)((data[15] << 8) | data[14]);
    dev.calib.dig_P6 = (s16)((data[17] << 8) | data[16]);
    dev.calib.dig_P7 = (s16)((data[19] << 8) | data[18]);
    dev.calib.dig_P8 = (s16)((data[21] << 8) | data[20]);
    dev.calib.dig_P9 = (s16)((data[23] << 8) | data[22]);
    
    dev.calib.dig_H1 = data[24];
    dev.calib.dig_H2 = (s16)((data[26] << 8) | data[25]);
    dev.calib.dig_H3 = data[27];
    dev.calib.dig_H4 = (s16)((data[28] << 4) | (data[29] & 0x0F));
    dev.calib.dig_H5 = (s16)((data[30] << 4) | ((data[29] >> 4) & 0x0F));
    dev.calib.dig_H6 = (s8)data[31];
    
    dev.calib_valid = true;
    return 0;
}

/* 数据补偿算法 */
static s32 compensate_temp(s32 adc_T) {
    if (!dev.calib_valid) return 0;
    
    s32 var1, var2;
    var1 = (((adc_T >> 3) - ((s32)dev.calib.dig_T1 << 1)) * 
           (s32)dev.calib.dig_T2) >> 11;
    var2 = (((((adc_T >> 4) - (s32)dev.calib.dig_T1) * 
            ((adc_T >> 4) - (s32)dev.calib.dig_T1)) >> 12) * 
            (s32)dev.calib.dig_T3) >> 14;
    dev.calib.t_fine = var1 + var2;
    return (dev.calib.t_fine * 5 + 128) >> 8;
}

static u32 compensate_press(s32 adc_P) {
    if (!dev.calib_valid) return 0;
    
    s64 var1, var2, p;
    var1 = (s64)dev.calib.t_fine - 128000;
    var2 = var1 * var1 * (s64)dev.calib.dig_P6;
    var2 += ((var1 * (s64)dev.calib.dig_P5) << 17);
    var2 += ((s64)dev.calib.dig_P4 << 35);
    var1 = ((var1 * var1 * (s64)dev.calib.dig_P3) >> 8) + 
           ((var1 * (s64)dev.calib.dig_P2) << 12);
    var1 = ((((s64)1 << 47) + var1) * (s64)dev.calib.dig_P1) >> 33;
    
    if (var1 == 0) return 0;
    
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = ((s64)dev.calib.dig_P9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = ((s64)dev.calib.dig_P8 * p) >> 19;
    return (u32)((p + var1 + var2) >> 8) + ((s64)dev.calib.dig_P7 << 4);
}

static u32 compensate_hum(s32 adc_H) {
    if (!dev.calib_valid) return 0;
    
    s32 var = dev.calib.t_fine - 76800;
    var = ((((adc_H << 14) - ((s32)dev.calib.dig_H4 << 20) - 
          (dev.calib.dig_H5 * var)) + 16384) >> 15) * 
          (((((((var * dev.calib.dig_H6) >> 10) * 
          ((var * dev.calib.dig_H3) >> 11 + 32768)) >> 10) + 2097152) * 
          dev.calib.dig_H2 + 8192) >> 14);
    
    var -= (((var >> 15) * (var >> 15)) >> 7) * dev.calib.dig_H1 >> 4;
    var = var < 0 ? 0 : (var > 419430400 ? 419430400 : var);
    return var >> 12;
}

/* 数据读取 */
static int bme280_read_raw(s32 *temp, s32 *press, s32 *hum) {
    u8 data[8];
    int ret = i2c_read_regs(BME280_PRESS_MSB, data, 8);
    if (ret != 8)
        return -EIO;
    
    *press = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
    *temp = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);
    *hum = (data[6] << 8) | data[7];
    return 0;
}

struct bme280_data {
    s32 temp;    // ℃ 
    u32 press;   // Pa
    u32 hum;     // % 
    
};

static int i2c_open(struct inode *inode, struct file *filp) {
    return 0;
}

static int i2c_release(struct inode *inode, struct file *filp) {
    return 0;
}

static ssize_t i2c_read(struct file *filp, char __user *buf, 
                       size_t count, loff_t *fpos) {
    struct bme280_data data;
    s32 raw_temp, raw_press, raw_hum;
    int ret;
    
    if (!dev.calib_valid)
        return -ENODEV;
    
    ret = bme280_read_raw(&raw_temp, &raw_press, &raw_hum);
    if (ret < 0)
        return ret;
    
    data.temp = compensate_temp(raw_temp);
    data.press = compensate_press(raw_press);
    data.hum = compensate_hum(raw_hum);
    
    if (copy_to_user(buf, &data, sizeof(data)))
        return -EFAULT;
    
    return sizeof(data);
}

static const struct file_operations i2c_fops = {
    .owner = THIS_MODULE,
    .open = i2c_open,
    .release = i2c_release,
    .read = i2c_read,
};

/* 模块初始化 */
static int __init bme280_init(void) {
    int ret;
    u8 chip_id;
    
    mutex_init(&dev.lock);
    dev.calib_valid = false;
    
    /* 申请GPIO */
    if ((ret = gpio_request(SCL_GPIO, "BME280_SCL")) != 0)
        goto fail;
    if ((ret = gpio_request(SDA_GPIO, "BME280_SDA")) != 0)
        goto free_scl;
    
    /* 配置GPIO方向 */
    if ((ret = gpio_direction_output(SCL_GPIO, 1)) != 0 ||
        (ret = gpio_direction_output(SDA_GPIO, 1)) != 0)
        goto free_gpio;
    
    /* 检查芯片ID */
    if (i2c_read_regs(BME280_CHIP_ID_REG, &chip_id, 1) != 1 || 
        chip_id != 0x60) {
        printk(KERN_ERR "无效的BME280芯片ID: 0x%02X\n", chip_id);
        ret = -ENODEV;
        goto free_gpio;
    }
    
    /* 初始化传感器 */
    if (i2c_write_reg(BME280_CTRL_HUM, 0x05) ||
        i2c_write_reg(BME280_CTRL_MEAS, 0xB7) ||
        i2c_write_reg(BME280_CONFIG, 0xA0)) {
        ret = -EIO;
        goto free_gpio;
    }
    
    /* 读取校准参数 */
    if ((ret = bme280_read_calib()) != 0)
        goto free_gpio;
    
    /* 注册字符设备 */
    if ((ret = alloc_chrdev_region(&i2c_dev, 0, 1, "bme280")) != 0)
        goto free_gpio;
    major = MAJOR(i2c_dev);
    
    if ((i2c_class = class_create(THIS_MODULE, "bme280")) == NULL) {
        ret = -ENOMEM;
        goto unreg_chrdev;
    }
    
    if ((i2c_device = device_create(i2c_class, NULL, i2c_dev, NULL, "bme280")) == NULL) {
        ret = -ENOMEM;
        goto destroy_class;
    }
    
    cdev_init(&i2c_cdev, &i2c_fops);
    if ((ret = cdev_add(&i2c_cdev, i2c_dev, 1)) != 0)
        goto destroy_device;
    
    printk(KERN_INFO "BME280驱动加载成功\n");
    return 0;

destroy_device:
    device_destroy(i2c_class, i2c_dev);
destroy_class:
    class_destroy(i2c_class);
unreg_chrdev:
    unregister_chrdev_region(i2c_dev, 1);
free_gpio:
    gpio_free(SCL_GPIO);
    gpio_free(SDA_GPIO);
free_scl:
    gpio_free(SCL_GPIO);
fail:
    return ret;
}

static void __exit bme280_exit(void) {
    cdev_del(&i2c_cdev);
    device_destroy(i2c_class, i2c_dev);
    class_destroy(i2c_class);
    unregister_chrdev_region(i2c_dev, 1);
    gpio_free(SCL_GPIO);
    gpio_free(SDA_GPIO);
    printk(KERN_INFO "BME280驱动卸载\n");
}

module_init(bme280_init);
module_exit(bme280_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alen");
MODULE_DESCRIPTION("BME280 I2C Sensor Driver");