#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define GPIO_NUM 60
#define SYSFS_GPIO_DIR "/sys/class/gpio"

// 检查GPIO是否已导出
static int gpio_is_exported() {
    char path[50];
    snprintf(path, sizeof(path), "%s/gpio%d", SYSFS_GPIO_DIR, GPIO_NUM);
    return access(path, F_OK) == 0;
}

// 导出GPIO（改进版）
static int gpio_export() {
    // 如果已经导出则直接返回成功
    if (gpio_is_exported()) {
        return 0;
    }

    int fd = open(SYSFS_GPIO_DIR "/export", O_WRONLY);
    if (fd < 0) {
        perror("❌ 导出GPIO失败 (export)");
        return -1;
    }

    char num_str[4];
    snprintf(num_str, sizeof(num_str), "%d", GPIO_NUM);
    if (write(fd, num_str, strlen(num_str)) < 0) {
        perror("❌ 写入GPIO编号失败");
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

// 设置GPIO方向
static int gpio_set_direction() {
    char path[50];
    snprintf(path, sizeof(path), "%s/gpio%d/direction", SYSFS_GPIO_DIR, GPIO_NUM);
    
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("❌ 设置方向失败 (direction)");
        return -1;
    }
    
    if (write(fd, "out", 3) < 0) {
        perror("❌ 设置为输出模式失败");
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

// 设置GPIO电平
static int gpio_set_value(int value) {
    char path[50];
    snprintf(path, sizeof(path), "%s/gpio%d/value", SYSFS_GPIO_DIR, GPIO_NUM);
    
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("❌ 设置电平失败 (value)");
        return -1;
    }
    
    char val = value ? '1' : '0';
    if (write(fd, &val, 1) < 0) {
        perror("❌ 写入电平值失败");
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("🔧 用法: %s <on|off>\n", argv[0]);
        printf("示例:\n");
        printf("  %s on   # 开启蜂鸣器\n", argv[0]);
        printf("  %s off  # 关闭蜂鸣器\n", argv[0]);
        return 1;
    }

    int set_high = 0;
    if (strcmp(argv[1], "on") == 0) {
        set_high = 0;
    } else if (strcmp(argv[1], "off") == 0) {
        set_high = 1;
    } else {
        fprintf(stderr, "❌ 无效参数，请使用 'on' 或 'off'\n");
        return 1;
    }

    printf("🛠️ 正在配置GPIO%d...\n", GPIO_NUM);
    
    // 导出GPIO（如果未导出）
    if (gpio_export() != 0) return 1;
    
    // 设置方向
    if (gpio_set_direction() != 0) return 1;
    
    printf("⚡ 正在%s蜂鸣器...\n", set_high ? "开启" : "关闭");
    if (gpio_set_value(set_high) != 0) return 1;
    
    printf("✅ 操作成功完成！\n");
    
    // 注意：这里特意不取消导出GPIO
    return 0;
}