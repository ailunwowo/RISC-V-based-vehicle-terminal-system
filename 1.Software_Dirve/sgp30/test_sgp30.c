#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <poll.h>

#define DEV_PATH "/dev/sgp30"

int main() {
    int fd, ret;
    char buffer[64];
    struct pollfd fds;

    if ((fd = open(DEV_PATH, O_RDONLY)) < 0) {
        fprintf(stderr, "打开设备失败: %s\n", strerror(errno));
        return -1;
    }

    fds.fd = fd;
    fds.events = POLLIN;

    while (1) {
        ret = poll(&fds, 1, 5000); // 5秒超时
        if (ret < 0) {
            perror("poll错误");
            break;
        } else if (ret == 0) {
            printf("等待传感器就绪...\n");
            continue;
        }

        if (fds.revents & POLLIN) {
            lseek(fd, 0, SEEK_SET);
            ret = read(fd, buffer, sizeof(buffer));
            if (ret < 0) {
                if (errno == EAGAIN) {
                    printf("传感器未就绪\n");
                    sleep(1);
                    continue;
                }
                perror("读取错误");
                break;
            }
            printf("传感器数据: %.*s", ret, buffer);
        }

        sleep(1);
    }

    close(fd);
    return 0;
}