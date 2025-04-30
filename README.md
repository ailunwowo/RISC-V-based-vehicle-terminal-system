# 基于车载电子设备的智能座舱终端设计
欢迎体验 **基于车载电子设备的智能座舱终端设计** 项目！本项目旨在通过硬件、软件和物联网云平台的集成，打造一个智能座舱系统，提升乘客舒适度并实现更智能化的控制。

---

## 1.项目介绍
2025/03/22 创建项目，确定基本目标

## 2.实现功能
### 2.1 驱动部分
- [√] PWM 风扇驱动
- [ ] LED灯带驱动
- [√] PWM舵机驱动
- [√] 电机驱动
- [√] BME280 温湿度压力传感器驱动
- [√] SGP30 气体传感器驱动
- [ ] GPS 传感器驱动
- [ ] 重力加速度传感器驱动
- [ ] USB摄像头驱动

### 2.2 应用部分
- [√] LVGL 移植
- [√] 动画库移植
- [ ] 音频部分demo
- [ ] gps demo
- [ ] usb摄像头demo

## 3.硬件开发环境
+ 硬件：昉·惊鸿-7110 开发板（VisionFive 2，简称 VF2）
    - 规格：四核 64 位 RV64GC ISA，最高频率 1.5 GHz，集成 IMG BXE-4-32 GPU，支持 OpenCL 3.0、OpenGL ES 3.2 和 Vulkan 1.2。
    - 内存：提供 2/4/8 GB LPDDR4 RAM 选项。
    - 接口：M.2 接口、eMMC 插座、USB 3.0 接口、40-pin GPIO、千兆以太网接口、TF 卡插槽、MIPI-CSI 和 MIPI-DSI。
    - 操作系统支持：官方适配 Debian，社区支持 Ubuntu、OpenSUSE、OpenKylin、OpenEuler、Deepin 等。
+ 用途：数据整合与控制的核心处理单元。

## 4.软件开发环境
Ubuntu22.04 

riscv64-buildroot-linux-gnu-gcc

```plain
NAME=Buildroot
VERSION=JH7110_VF2_515_v5.13.1-dirty
ID=buildroot
VERSION_ID=2021.11
PRETTY_NAME="Buildroot 2021.11"
```

JH7110_VisionFive2_devel：[https://github.com/starfive-tech/VisionFive2](https://github.com/starfive-tech/VisionFive2)

release/v9.2：[https://github.com/lvgl/lvgl/tree/release/v9.2](https://github.com/lvgl/lvgl/tree/release/v9.2)



