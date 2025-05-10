#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

// 打开并配置串口
int open_serial_port(const char *port) {
    int fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
        perror("Unable to open serial port");
        return -1;
    }

    struct termios options;
    tcgetattr(fd, &options);
    cfsetispeed(&options, B38400);
    cfsetospeed(&options, B38400);
    options.c_cflag = (options.c_cflag & ~CSIZE) | CS8; // 8位数据
    options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    options.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    options.c_oflag &= ~OPOST;
    options.c_cflag &= ~(PARENB | CSTOPB); // 无奇偶校验，1停止位
    options.c_cflag |= CLOCAL | CREAD;
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 10; // 超时1秒

    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        perror("Failed to set serial attributes");
        close(fd);
        return -1;
    }

    tcflush(fd, TCIFLUSH); // 清空输入缓冲区
    return fd;
}

// 解析 GPGLL 语句并按指定格式打印经纬度
void parse_gpgll(const char *line) {
    char latitude[16], lat_dir[2], longitude[16], lon_dir[2], status[2];

    // 分割 GPGLL 字段（格式: $GPGLL,lat,lat_dir,lon,lon_dir,time,status,*checksum）
    int parsed = sscanf(line, "$GPGLL,%[^,],%[^,],%[^,],%[^,],%*[^,],%[^,*]", 
                        latitude, lat_dir, longitude, lon_dir, status);

    if (parsed < 5) {
        printf("Invalid GPGLL sentence: %s\n", line);
        return;
    }

    if (status[0] != 'A') {
        printf("GPS signal not acquired (Invalid fix)\n");
        return;
    }

    // 计算纬度（格式: DDMM.MMMM）
    double lat_value;
    sscanf(latitude, "%lf", &lat_value);
    int lat_degrees = (int)(lat_value / 100); // 度
    double lat_minutes = lat_value - (lat_degrees * 100); // 分
    double lat_seconds = (lat_minutes - (int)lat_minutes) * 60; // 秒

    // 计算经度（格式: DDDMM.MMMM）
    double lon_value;
    sscanf(longitude, "%lf", &lon_value);
    int lon_degrees = (int)(lon_value / 100); // 度
    double lon_minutes = lon_value - (lon_degrees * 100); // 分
    double lon_seconds = (lon_minutes - (int)lon_minutes) * 60; // 秒

    // 打印经纬度
    printf("纬度: %d° %d' %.4f\" %s\n", lat_degrees, (int)lat_minutes, lat_seconds, lat_dir);
    printf("经度: %d° %d' %.4f\" %s\n", lon_degrees, (int)lon_minutes, lon_seconds, lon_dir);
}

int main() {
    const char *port = "/dev/ttyS3";
    int fd = open_serial_port(port);
    if (fd < 0) {
        return 1;
    }

    char buffer[256];
    int buf_pos = 0;

    printf("Reading GPS data from %s...\n", port);

    while (1) {
        char c;
        int n = read(fd, &c, 1);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(10000); // 短暂休眠 10ms，避免 CPU 占用过高
                continue; // 非阻塞模式下无数据，正常继续
            }
            perror("Read error");
            close(fd);
            return 1;
        } else if (n == 0) {
            continue; // 超时，继续读取
        }

        if (c == '\n' || c == '\r') {
            if (buf_pos > 0) {
                buffer[buf_pos] = '\0';
                if (strncmp(buffer, "$GPGLL", 6) == 0) {
                    parse_gpgll(buffer);
                }
                buf_pos = 0;
            }
        } else {
            if (buf_pos < sizeof(buffer) - 1) {
                buffer[buf_pos++] = c;
            } else {
                buf_pos = 0; // 防止缓冲区溢出
            }
        }
    }

    close(fd);
    return 0;
}