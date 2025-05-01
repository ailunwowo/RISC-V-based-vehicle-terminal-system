#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#define SPI_DEVICE    "/dev/spidev1.0"
#define SPI_SPEED_HZ  8000000   // 8 MHz（适配你的 SPI 控制器限制）
#define SPI_BITS      8
#define SPI_MODE      SPI_MODE_0

// WS2812B 协议参数
#define T0H  0x60  // '0' bit: 01100000 (占空比 2/8)
#define T1H  0x7C  // '1' bit: 01111100 (占空比 6/8)
#define RESET_DELAY 64  // Reset 信号长度（单位：字节，全0）

// 将 RGB 转换为 WS2812B 的 SPI 数据格式
void rgb_to_spi(uint8_t r, uint8_t g, uint8_t b, uint8_t *spi_buf) {
    uint8_t grb[3] = {g, r, b};  // WS2812B 使用 GRB 顺序
    for (int i = 0; i < 24; i++) {
        uint8_t bit = (grb[i / 8] >> (7 - (i % 8))) & 0x01;
        spi_buf[i] = bit ? T1H : T0H;
    }
}

int main() {
    int spi_fd;
    uint8_t mode = SPI_MODE;
    uint8_t bits = SPI_BITS;
    uint32_t speed = SPI_SPEED_HZ;

    // 打开 SPI 设备
    if ((spi_fd = open(SPI_DEVICE, O_RDWR)) < 0) {
        perror("Failed to open SPI device");
        return -1;
    }

    // 配置 SPI 参数
    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0) {
        perror("Failed to set SPI mode");
        close(spi_fd);
        return -1;
    }
    if (ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        perror("Failed to set bits per word");
        close(spi_fd);
        return -1;
    }
    if (ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        perror("Failed to set max speed");
        close(spi_fd);
        return -1;
    }

    // 主循环：红 → 绿 → 蓝
    uint8_t reset_buf[RESET_DELAY];
    uint8_t spi_data[24];  // 24 bits (3 bytes) per LED
    uint8_t output_buf[RESET_DELAY + 24 + RESET_DELAY];  // Reset + Data + Reset

    memset(reset_buf, 0x00, sizeof(reset_buf));  // Reset 信号（全0）

    while (1) {
        // 红色
        rgb_to_spi(0xFF, 0x00, 0x00, spi_data);
        memset(output_buf, 0x00, sizeof(output_buf));
        memcpy(output_buf + RESET_DELAY, spi_data, 24);
        write(spi_fd, output_buf, sizeof(output_buf));
        usleep(500000);  // 500ms 延迟

        // 绿色
        rgb_to_spi(0x00, 0xFF, 0x00, spi_data);
        memset(output_buf, 0x00, sizeof(output_buf));
        memcpy(output_buf + RESET_DELAY, spi_data, 24);
        write(spi_fd, output_buf, sizeof(output_buf));
        usleep(500000);

        // 蓝色
        rgb_to_spi(0x00, 0x00, 0xFF, spi_data);
        memset(output_buf, 0x00, sizeof(output_buf));
        memcpy(output_buf + RESET_DELAY, spi_data, 24);
        write(spi_fd, output_buf, sizeof(output_buf));
        usleep(500000);
    }

    close(spi_fd);
    return 0;
}