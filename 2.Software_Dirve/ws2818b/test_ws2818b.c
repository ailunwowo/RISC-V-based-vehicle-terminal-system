#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

void ws281x_delay(long target_ns) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    do {
        clock_gettime(CLOCK_MONOTONIC, &end);
    } while ((end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec) < target_ns);
}

void PAout(int n) {
    char buf[64];
    sprintf(buf, "echo %d > /sys/class/gpio/gpio50/value", n);
    system(buf);
}

void ws281x_sendLow(void) {
    PAout(1);
    ws281x_delay(1  ); // 440ns
    PAout(0);
    ws281x_delay(2  ); // 880ns
}

void ws281x_sendHigh(void) {
    PAout(1);
    ws281x_delay(2  ); // 880ns
    PAout(0);
    ws281x_delay(1  ); // 440ns
}

void ws2811_Reset(void) {
    PAout(0);
    usleep(60); // 60μs
    PAout(1);
    PAout(0);
}

void ws281x_sendOne(uint32_t dat) {
    uint8_t i;
    unsigned char byte;
    for (i = 23; i >= 0; i--) { // 修正循环，从23到0，处理24位
        byte = ((dat >> i) & 0x01);
        if (byte == 1) {
            ws281x_sendHigh();
        } else {
            ws281x_sendLow();
        }
    }
}

int main() {
    ws281x_sendOne(0xFF0000); // 发送红色
    return 0;
}