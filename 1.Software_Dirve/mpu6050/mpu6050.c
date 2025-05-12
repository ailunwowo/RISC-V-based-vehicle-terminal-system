#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/i2c.h>
#include <asm/uaccess.h>
#include <asm/io.h>
/***************************************************************
文件名	  	: mpu6050i2c.c
作者	  	: 1997AURORA
版本	   	: V1.0
描述	   	: mpu6050 I2C驱动程序
其他	   	: 无
日志	   	: 初版V1.0 2021/8/8 1997AURORA创建
***************************************************************/
#define mpu6050i2c_CNT	1
#define mpu6050i2c_NAME	"mpu6050i2c"

struct mpu6050i2c_dev {
	dev_t devid;			/* 设备号 	 */
	struct cdev cdev;		/* cdev 	*/
	struct class *class;	/* 类 		*/
	struct device *device;	/* 设备 	 */
	struct device_node	*nd; /* 设备节点 */
	int major;			/* 主设备号 */
	void *private_data;	/* 私有数据 */
	int16_t acceleration[3], gyro[3], temp;		/* 芯片数据 */
};

static struct mpu6050i2c_dev mpu6050i2cdev;

/*
 * @description	: 从mpu6050i2c读取多个寄存器数据
 * @param - dev:  mpu6050i2c设备
 * @param - reg:  要读取的寄存器首地址
 * @param - val:  读取到的数据
 * @param - len:  要读取的数据长度
 * @return 		: 操作结果
 */
static int mpu6050i2c_read_regs(struct mpu6050i2c_dev *dev, u8 reg, void *val, int len)
{
	int ret;
	struct i2c_msg msg[2];
	struct i2c_client *client = (struct i2c_client *)dev->private_data;

	/* msg[0]为发送要读取的首地址 */
	msg[0].addr = client->addr;			/* mpu6050i2c地址 */
	msg[0].flags = 0;					/* 标记为发送数据 */
	msg[0].buf = &reg;					/* 读取的首地址 */
	msg[0].len = 1;						/* reg长度*/

	/* msg[1]读取数据 */
	msg[1].addr = client->addr;			/* mpu6050i2c地址 */
	msg[1].flags = I2C_M_RD;			/* 标记为读取数据*/
	msg[1].buf = val;					/* 读取数据缓冲区 */
	msg[1].len = len;					/* 要读取的数据长度*/

	ret = i2c_transfer(client->adapter, msg, 2);
	if(ret == 2) {
		ret = 0;
	} else {
		printk("i2c rd failed=%d reg=%06x len=%d\n",ret, reg, len);
		ret = -EREMOTEIO;
	}
	return ret;
}

/*
 * @description	: 向mpu6050i2c多个寄存器写入数据
 * @param - dev:  mpu6050i2c设备
 * @param - reg:  要写入的寄存器首地址
 * @param - val:  要写入的数据缓冲区
 * @param - len:  要写入的数据长度
 * @return 	  :   操作结果
 */
static s32 mpu6050i2c_write_regs(struct mpu6050i2c_dev *dev, u8 reg, u8 *buf, u8 len)
{
	u8 b[256];
	struct i2c_msg msg;
	struct i2c_client *client = (struct i2c_client *)dev->private_data;
	
	b[0] = reg;					/* 寄存器首地址 */
	memcpy(&b[1],buf,len);		/* 将要写入的数据拷贝到数组b里面 */
		
	msg.addr = client->addr;	/* mpu6050i2c地址 */
	msg.flags = 0;				/* 标记为写数据 */

	msg.buf = b;				/* 要写入的数据缓冲区 */
	msg.len = len + 1;			/* 要写入的数据长度 */

	return i2c_transfer(client->adapter, &msg, 1);
}

/*
 * @description	: 读取mpu6050i2c指定寄存器值，读取一个寄存器
 * @param - dev:  mpu6050i2c设备
 * @param - reg:  要读取的寄存器
 * @return 	  :   读取到的寄存器值
 */
static unsigned char mpu6050i2c_read_reg(struct mpu6050i2c_dev *dev, u8 reg)
{
	u8 data = 0;

	mpu6050i2c_read_regs(dev, reg, &data, 1);
	return data;

#if 0
	struct i2c_client *client = (struct i2c_client *)dev->private_data;
	return i2c_smbus_read_byte_data(client, reg);
#endif
}

/*
 * @description	: 向mpu6050i2c指定寄存器写入指定的值，写一个寄存器
 * @param - dev:  mpu6050i2c设备
 * @param - reg:  要写的寄存器
 * @param - data: 要写入的值
 * @return   :    无
 */
static void mpu6050i2c_write_reg(struct mpu6050i2c_dev *dev, u8 reg, u8 data)
{
	u8 buf = 0;
	buf = data;
	mpu6050i2c_write_regs(dev, reg, &buf, 1);
}

static void mpu6050_reset(void) {
	mpu6050i2c_write_reg(&mpu6050i2cdev, 0x6B, 0x00);
}
/*
 * @description	: 读取mpu6050i2c的数据，读取原始数据
 * @param - dev:  mpu6050i2c设备
 * @return 		: 无。
 */
void mpu6050i2c_readdata(struct mpu6050i2c_dev *dev)
{
	unsigned char i =0;
    unsigned char buf[6];
	unsigned char val = 0x3B;
	// Start reading acceleration registers from register 0x3B for 6 bytes
	mpu6050i2c_read_regs(dev, val, buf, 6);
	for(i = 0; i < 3; i++)	
	{
		dev->acceleration[i] = (buf[i * 2] << 8 | buf[(i * 2) + 1]);
	}
	// Now gyro data from reg 0x43 for 6 bytes
    // The register is auto incrementing on each read
	val = 0x43;
	mpu6050i2c_read_regs(dev, val, buf, 6);
	for(i = 0; i < 3; i++)	
	{
		dev->gyro[i] = (buf[i * 2] << 8 | buf[(i * 2) + 1]);
	}
	// Now temperature from reg 0x41 for 2 bytes
    // The register is auto incrementing on each read
	val = 0x41;
	mpu6050i2c_read_regs(dev, val, buf, 2);
	for(i = 0; i < 3; i++)	
	{
		dev->temp = buf[0] << 8 | buf[1];
	}
}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int mpu6050i2c_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &mpu6050i2cdev;
	return 0;
}

/*
 * @description		: 从设备读取数据 
 * @param - filp 	: 要打开的设备文件(文件描述符)
 * @param - buf 	: 返回给用户空间的数据缓冲区
 * @param - cnt 	: 要读取的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 读取的字节数，如果为负值，表示读取失败
 */
static ssize_t mpu6050i2c_read(struct file *filp, char __user *buf, size_t cnt, loff_t *off)
{
	int16_t data[7];
	long err = 0;

	struct mpu6050i2c_dev *dev = (struct mpu6050i2c_dev *)filp->private_data;
	
	mpu6050i2c_readdata(dev);

	data[0] = dev->acceleration[0];
	data[1] = dev->acceleration[1];
	data[2] = dev->acceleration[2];
	data[3] = dev->gyro[0];
	data[4] = dev->gyro[1];
	data[5] = dev->gyro[2];
	data[6] = dev->temp;
	err = copy_to_user(buf, data, sizeof(data));
	return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int mpu6050i2c_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* mpu6050i2c操作函数 */
static const struct file_operations mpu6050i2c_ops = {
	.owner = THIS_MODULE,
	.open = mpu6050i2c_open,
	.read = mpu6050i2c_read,
	.release = mpu6050i2c_release,
};

 /*
  * @description     : i2c驱动的probe函数，当驱动与
  *                    设备匹配以后此函数就会执行
  * @param - client  : i2c设备
  * @param - id      : i2c设备ID
  * @return          : 0，成功;其他负值,失败
  */
static int mpu6050i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	/* 1、构建设备号 */
	if (mpu6050i2cdev.major) {
		mpu6050i2cdev.devid = MKDEV(mpu6050i2cdev.major, 0);
		register_chrdev_region(mpu6050i2cdev.devid, mpu6050i2c_CNT, mpu6050i2c_NAME);
	} else {
		alloc_chrdev_region(&mpu6050i2cdev.devid, 0, mpu6050i2c_CNT, mpu6050i2c_NAME);
		mpu6050i2cdev.major = MAJOR(mpu6050i2cdev.devid);
	}

	/* 2、注册设备 */
	cdev_init(&mpu6050i2cdev.cdev, &mpu6050i2c_ops);
	cdev_add(&mpu6050i2cdev.cdev, mpu6050i2cdev.devid, mpu6050i2c_CNT);

	/* 3、创建类 */
	mpu6050i2cdev.class = class_create(THIS_MODULE, mpu6050i2c_NAME);
	if (IS_ERR(mpu6050i2cdev.class)) {
		return PTR_ERR(mpu6050i2cdev.class);
	}

	/* 4、创建设备 */
	mpu6050i2cdev.device = device_create(mpu6050i2cdev.class, NULL, mpu6050i2cdev.devid, NULL, mpu6050i2c_NAME);
	if (IS_ERR(mpu6050i2cdev.device)) {
		return PTR_ERR(mpu6050i2cdev.device);
	}

	mpu6050i2cdev.private_data = client;
	
	mpu6050_reset();
	printk("mpu6050_reset\n");

	return 0;
}

/*
 * @description     : i2c驱动的remove函数，移除i2c驱动的时候此函数会执行
 * @param - client 	: i2c设备
 * @return          : 0，成功;其他负值,失败
 */
static int mpu6050i2c_remove(struct i2c_client *client)
{
	/* 删除设备 */
	cdev_del(&mpu6050i2cdev.cdev);
	unregister_chrdev_region(mpu6050i2cdev.devid, mpu6050i2c_CNT);

	/* 注销掉类和设备 */
	device_destroy(mpu6050i2cdev.class, mpu6050i2cdev.devid);
	class_destroy(mpu6050i2cdev.class);
	return 0;
}

/* 传统匹配方式ID列表 */
static const struct i2c_device_id mpu6050i2c_id[] = {
	{"leapfive,mpu6050", 0},  
	{}
};

/* 设备树匹配列表 */
static const struct of_device_id mpu6050i2c_of_match[] = {
	{ .compatible = "leapfive,mpu6050" },
	{ /* Sentinel */ }
};

/* i2c驱动结构体 */	
static struct i2c_driver mpu6050i2c_driver = {
	.probe = mpu6050i2c_probe,
	.remove = mpu6050i2c_remove,
	.driver = {
			.owner = THIS_MODULE,
		   	.name = "mpu6050i2c",
		   	.of_match_table = mpu6050i2c_of_match, 
		   },
	.id_table = mpu6050i2c_id,
};
		   
/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init mpu6050i2c_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&mpu6050i2c_driver);
	return ret;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit mpu6050i2c_exit(void)
{
	i2c_del_driver(&mpu6050i2c_driver);
}

/* module_i2c_driver(mpu6050i2c_driver) */

module_init(mpu6050i2c_init);
module_exit(mpu6050i2c_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alen");
