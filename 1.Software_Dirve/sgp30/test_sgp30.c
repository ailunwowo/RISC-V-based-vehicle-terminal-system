#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd = open("/dev/sgp30", O_RDONLY);
    if (fd < 0) {
        perror("打开设备失败");
        return -1;
    }

    char buffer[64];
    while (1)
    {
        ssize_t ret = read(fd, buffer, sizeof(buffer));
        if (ret > 0) {
            printf("传感器数据: %s", buffer);
        } else {
            perror("读取失败");
        }
        sleep(1);
    }
    close(fd);
    return 0;
}