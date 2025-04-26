#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

struct bme280_data {
    int temp;
    unsigned int press;
    unsigned int hum;
};

int main() {
    int fd = open("/dev/bme280", O_RDONLY);
    if (fd < 0) {
        perror("设备打开失败");
        return -1;
    }
    
    struct bme280_data data;
    while(1) {
        ssize_t ret = read(fd, &data, sizeof(data));
        if (ret != sizeof(data)) {
            if (errno == EAGAIN) continue;
            perror("读取失败");
            sleep(1);
            continue;
        }
        
        printf("温度: %.2f℃\n", data.temp / 100.0);
        printf("气压: %.2fhPa\n", data.press / 25600.0);
        printf("湿度: %.2f%%\n\n", data.hum / 1024.0);
        sleep(1);
    }
    
    close(fd);
    return 0;
}