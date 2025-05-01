#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#define SPI_DEVICE    "/dev/spidev1.0"
#define LED_COUNT     10       // 总共10个LED灯珠
#define SPI_SPEED_HZ  8000000  // 8 MHz
#define SPI_BITS      8
#define SPI_MODE      SPI_MODE_0
#define RESET_DELAY   64       // Reset信号长度

// WS2812B协议参数
#define T0H  0x60  // '0' bit: 01100000
#define T1H  0x7C  // '1' bit: 01111100

int spi_fd;

// 将RGB转换为WS2812B的SPI数据格式
void rgb_to_spi(uint8_t r, uint8_t g, uint8_t b, uint8_t *spi_buf) {
    uint8_t grb[3] = {g, r, b};  // WS2812B使用GRB顺序
    for (int i = 0; i < 24; i++) {
        uint8_t bit = (grb[i / 8] >> (7 - (i % 8))) & 0x01;
        spi_buf[i] = bit ? T1H : T0H;
    }
}

// 初始化SPI
int init_spi() {
    uint8_t mode = SPI_MODE;
    uint8_t bits = SPI_BITS;  // 将宏定义赋值给变量
    uint32_t speed = SPI_SPEED_HZ;

    if ((spi_fd = open(SPI_DEVICE, O_RDWR)) < 0) {
        perror("Failed to open SPI device");
        return -1;
    }

    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0 ||
        ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||  // 使用变量地址
        ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        perror("Failed to set SPI parameters");
        close(spi_fd);
        return -1;
    }

    return 0;
}

// 控制单个LED
void set_led(int led_pos, uint8_t r, uint8_t g, uint8_t b) {
    static uint8_t led_data[LED_COUNT][24] = {0};
    uint8_t output_buf[RESET_DELAY + (LED_COUNT * 24) + RESET_DELAY];
    uint8_t reset_buf[RESET_DELAY] = {0};

    if (led_pos >= 0 && led_pos < LED_COUNT) {
        rgb_to_spi(r, g, b, led_data[led_pos]);
    }

    memset(output_buf, 0x00, sizeof(output_buf));
    memcpy(output_buf + RESET_DELAY, led_data, sizeof(led_data));
    
    write(spi_fd, reset_buf, sizeof(reset_buf));
    write(spi_fd, output_buf, sizeof(output_buf));
    write(spi_fd, reset_buf, sizeof(reset_buf));
}

// 清除所有LED
void clear_all() {
    for (int i = 0; i < LED_COUNT; i++) {
        set_led(i, 0x00, 0x00, 0x00);
    }
}

int main() {
    if (init_spi() < 0) return 1;

    clear_all();

    set_led(0, 100, 0x00, 0x00);
    sleep(1);
    set_led(1, 200, 0x00, 0x00);
    sleep(1);
    set_led(2, 250, 0x00, 0x00);
    sleep(1);

    clear_all();
    close(spi_fd);
    return 0;
}