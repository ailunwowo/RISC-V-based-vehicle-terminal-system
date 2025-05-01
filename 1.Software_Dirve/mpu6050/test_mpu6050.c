#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "sys/ioctl.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include <poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
/***************************************************************
文件名	  	: mpu6050i2cApp.c
作者	  	: 1997AURORA
版本	   	: V1.0
描述	   	: mpu6050 I2C应用程序
其他	   	: 无
日志	   	: 初版V1.0 2021/8/8 1997AURORA创建
***************************************************************/

/*
 * @description		: main主程序
 * @param - argc 	: argv数组元素个数
 * @param - argv 	: 具体参数
 * @return 			: 0 成功;其他 失败
 */
int main(int argc, char *argv[])
{
	int fd;
	char *filename;
	int16_t databuf[7];
	int16_t gyro_x_adc, gyro_y_adc, gyro_z_adc;
	int16_t accel_x_adc, accel_y_adc, accel_z_adc;
	int16_t temp_adc;

	int ret = 0;

	if (argc != 2) {
		printf("Error Usage!\r\n");
		return -1;
	}

	filename = argv[1];
	fd = open(filename, O_RDWR);
	if(fd < 0) {
		printf("can't open file %s\r\n", filename);
		return -1;
	}

	while (1) {
		ret = read(fd, databuf, sizeof(databuf));
		if(ret == 0) { 			/* 数据读取成功 */
			accel_x_adc = databuf[0];
			accel_y_adc = databuf[1];
			accel_z_adc = databuf[2];
			gyro_x_adc = databuf[3];
			gyro_y_adc = databuf[4];
			gyro_z_adc = databuf[5];
			temp_adc = databuf[6];

			// These are the raw numbers from the chip, so will need tweaking to be really useful.
			// See the datasheet for more information
			printf("Acc. X = %d, Y = %d, Z = %d\n", accel_x_adc, accel_y_adc, accel_z_adc);
			printf("Gyro. X = %d, Y = %d, Z = %d\n", gyro_x_adc, gyro_y_adc, gyro_z_adc);
			// Temperature is simple so use the datasheet calculation to get deg C.
			// Note this is chip temperature.
			printf("Temp. = %f\n", (temp_adc / 340.0) + 36.53);
		}
		usleep(1000000); /*1000ms */
	}
	close(fd);	/* 关闭文件 */	
	return 0;
}
