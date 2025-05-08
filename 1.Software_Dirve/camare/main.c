#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <linux/fb.h>
#include <jpeglib.h>
#include <stdint.h>
#include <time.h>

#define FB_DEVICE "/dev/fb0"
#define VIDEO_DEVICE "/dev/video4"
#define WIDTH 320
#define HEIGHT 240
#define FPS 30
#define FRAME_INTERVAL_NS (1000000000 / FPS)

void rgb_to_framebuffer(uint8_t *rgb, uint32_t *fb_mem) {
    int x_offset = 361; // 指定 x 坐标
    int y_offset = 122; // 指定 y 坐标
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int rgb_idx = (y * WIDTH + x) * 3;
            int fb_idx = ((y + y_offset) * 720 + (x + x_offset));
            uint8_t r = rgb[rgb_idx], g = rgb[rgb_idx + 1], b = rgb[rgb_idx + 2];
            fb_mem[fb_idx] = (b << 0) | (g << 8) | (r << 16); // BGRA
        }
    }
}

int main() {
    // 打开设备
    int v4l2_fd = open(VIDEO_DEVICE, O_RDWR);
    int fb_fd = open(FB_DEVICE, O_RDWR);
    if (v4l2_fd < 0 || fb_fd < 0) {
        perror("Open device failed");
        return -1;
    }

    // 设置 V4L2 格式和帧率
    struct v4l2_format fmt = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE, .fmt.pix.width = WIDTH, .fmt.pix.height = HEIGHT, .fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG};
    struct v4l2_streamparm parm = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE, .parm.capture.timeperframe.numerator = 1, .parm.capture.timeperframe.denominator = FPS};
    ioctl(v4l2_fd, VIDIOC_S_FMT, &fmt);
    ioctl(v4l2_fd, VIDIOC_S_PARM, &parm);

    // 请求和映射 V4L2 缓冲区
    struct v4l2_requestbuffers req = {.count = 4, .type = V4L2_BUF_TYPE_VIDEO_CAPTURE, .memory = V4L2_MEMORY_MMAP};
    ioctl(v4l2_fd, VIDIOC_REQBUFS, &req);
    void *buffers[4];
    size_t *buffer_lengths = calloc(req.count, sizeof(size_t));
    for (int i = 0; i < req.count; i++) {
        struct v4l2_buffer buf = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE, .memory = V4L2_MEMORY_MMAP, .index = i};
        ioctl(v4l2_fd, VIDIOC_QUERYBUF, &buf);
        buffers[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, v4l2_fd, buf.m.offset);
        buffer_lengths[i] = buf.length;
        ioctl(v4l2_fd, VIDIOC_QBUF, &buf);
    }

    // 启动 V4L2 捕获
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(v4l2_fd, VIDIOC_STREAMON, &type);

    // 映射帧缓冲区
    uint32_t *fb_mem = mmap(NULL, 720 * 480 * 4, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);

    // JPEG 解码器
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);

    // 主循环
    struct timespec next_frame, now;
    clock_gettime(CLOCK_MONOTONIC, &next_frame);
    while (1) {
        struct v4l2_buffer buf = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE, .memory = V4L2_MEMORY_MMAP};
        ioctl(v4l2_fd, VIDIOC_DQBUF, &buf);

        jpeg_mem_src(&cinfo, buffers[buf.index], buf.bytesused);
        jpeg_read_header(&cinfo, TRUE);
        cinfo.out_color_space = JCS_RGB;
        jpeg_start_decompress(&cinfo);
        uint8_t *rgb = malloc(WIDTH * HEIGHT * 3);
        JSAMPARRAY row_pointer = malloc(sizeof(JSAMPROW));
        row_pointer[0] = rgb;
        while (cinfo.output_scanline < HEIGHT) {
            row_pointer[0] = rgb + cinfo.output_scanline * WIDTH * 3;
            jpeg_read_scanlines(&cinfo, row_pointer, 1);
        }
        jpeg_finish_decompress(&cinfo);

        rgb_to_framebuffer(rgb, fb_mem);
        free(rgb);
        free(row_pointer);
        ioctl(v4l2_fd, VIDIOC_QBUF, &buf);

        next_frame.tv_nsec += FRAME_INTERVAL_NS;
        if (next_frame.tv_nsec >= 1000000000) {
            next_frame.tv_sec++;
            next_frame.tv_nsec -= 1000000000;
        }
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec < next_frame.tv_sec || (now.tv_sec == next_frame.tv_sec && now.tv_nsec < next_frame.tv_nsec)) {
            struct timespec sleep_time = {
                .tv_sec = next_frame.tv_sec - now.tv_sec,
                .tv_nsec = next_frame.tv_nsec - now.tv_nsec
            };
            if (sleep_time.tv_nsec < 0) {
                sleep_time.tv_sec--;
                sleep_time.tv_nsec += 1000000000;
            }
            nanosleep(&sleep_time, NULL);
        }
    }

    // 清理
    jpeg_destroy_decompress(&cinfo);
    ioctl(v4l2_fd, VIDIOC_STREAMOFF, &type);
    for (int i = 0; i < req.count; i++) munmap(buffers[i], buffer_lengths[i]);
    free(buffers[0]);
    free(buffer_lengths);
    munmap(fb_mem, 720 * 480 * 4);
    close(fb_fd);
    close(v4l2_fd);
    return 0;
}