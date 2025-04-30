#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <stdint.h>
#define GPIO_PATH "/sys/class/gpio/gpio50/value"
#define T0H_NS  50    // 0码高电平时间
#define T1H_NS  900    // 1码高电平时间
#define CYCLE_NS 950  // 码元总周期
#define NS_PER_SEC 1000000000L

int gpio_fd;

// 函数前置声明
void busy_delay_ns(long ns);
void set_realtime_priority();

// GPIO电平控制
void gpio_write(int val) {
    char buf[2] = {val ? '1' : '0', '\n'};
    if (write(gpio_fd, buf, sizeof(buf)) != sizeof(buf)) {
        perror("GPIO write error");
    }
    lseek(gpio_fd, 0, SEEK_SET);
}

// 精准延时实现
void busy_delay_ns(long ns) {
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    do {
        clock_gettime(CLOCK_MONOTONIC, &now);
    } while ((now.tv_sec - start.tv_sec)*NS_PER_SEC 
           + (now.tv_nsec - start.tv_nsec) < ns);
}

// 设置实时优先级
void set_realtime_priority() {
    struct sched_param param = {.sched_priority = 99};
    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
        perror("sched_setscheduler failed");
    }
}

// 发送单个比特
void send_bit(int val) {
    gpio_write(1);
    busy_delay_ns(val ? T1H_NS : T0H_NS);
    gpio_write(0);
    busy_delay_ns(CYCLE_NS - (val ? T1H_NS : T0H_NS));
}

// 发送24位颜色数据
void send_grb(uint8_t g, uint8_t r, uint8_t b) {
    for(int i=7; i>=0; i--) send_bit((g >> i) & 1);
    for(int i=7; i>=0; i--) send_bit((r >> i) & 1);
    for(int i=7; i>=0; i--) send_bit((b >> i) & 1);
}

int main() {
    set_realtime_priority();
    
    if((gpio_fd = open(GPIO_PATH, O_WRONLY)) < 0) {
        perror("GPIO open failed");
        exit(EXIT_FAILURE);
    }

    // 点亮10个蓝色LED
    for(int i=0; i<4; i++) 
        send_grb(0x00, 0x00, 0xFF);
    
    close(gpio_fd);
    return EXIT_SUCCESS;
}