#!/bin/bash

# PWM 设备路径
PWM_PATH="/sys/class/pwm/pwmchip0/pwm1"

# 检查 PWM 路径是否存在
if [ ! -d "$PWM_PATH" ]; then
    echo "错误：PWM 路径 $PWM_PATH 不存在！请检查设备或导出 PWM 通道。"
    exit 1
fi

# PWM 参数
PERIOD=20000000  # 20ms (50Hz)
MID_DUTY=1500000 # 1.5ms (停止)
MIN_DUTY=500000  # 0.5ms (最大 CCW 速度)
MAX_DUTY=2500000 # 2.5ms (最大 CW 速度)

# 初始化 PWM
init_pwm() {
    # 禁用 PWM
    echo 0 > "$PWM_PATH/enable" 2>/dev/null

    # 设置周期
    echo $PERIOD > "$PWM_PATH/period" || {
        echo "错误：无法设置周期！"
        return 1
    }

    # 设置初始占空比（停止）
    echo $MID_DUTY > "$PWM_PATH/duty_cycle" || {
        echo "错误：无法设置占空比！"
        return 1
    }

    # 设置极性
    echo "normal" > "$PWM_PATH/polarity" || {
        echo "错误：无法设置极性！"
        return 1
    }

    # 启用 PWM
    echo 1 > "$PWM_PATH/enable" || {
        echo "错误：无法启用 PWM！"
        return 1
    }

    echo "PWM 初始化成功，舵机已停止。"
    return 0
}

# 设置舵机旋转
set_servo() {
    local direction=$1
    local speed=$2
    local duration=$3

    # 计算占空比
    if [ "$direction" = "cw" ]; then
        DUTY_CYCLE=$((MID_DUTY + (speed * (MAX_DUTY - MID_DUTY) / 100)))
    else
        DUTY_CYCLE=$((MID_DUTY - (speed * (MID_DUTY - MIN_DUTY) / 100)))
    fi

    # 设置占空比
    echo $DUTY_CYCLE > "$PWM_PATH/duty_cycle" || {
        echo "错误：无法设置占空比！"
        return 1
    }

    echo "成功：舵机设置为 $direction 方向，速度 $speed%，占空比 $DUTY_CYCLE 纳秒。"

    # 如果指定了持续时间，等待后停止
    if [ -n "$duration" ]; then
        sleep $duration
        echo $MID_DUTY > "$PWM_PATH/duty_cycle" || {
            echo "错误：无法停止舵机！"
            return 1
        }
        echo "舵机已停止（占空比设置为 $MID_DUTY 纳秒）。"
    fi

    return 0
}

# 停止舵机
stop_servo() {
    echo $MID_DUTY > "$PWM_PATH/duty_cycle" || {
        echo "错误：无法停止舵机！"
        return 1
    }
    echo "舵机已停止（占空比设置为 $MID_DUTY 纳秒）。"
    return 0
}

# 使用说明
usage() {
    echo "用法: $0 [cw|ccw] <速度 0-100> [时间(秒)]"
    echo "  或: $0 stop"
    echo "示例:"
    echo "  $0 cw 50      # 顺时针方向，速度50%持续运行"
    echo "  $0 ccw 30 1.5 # 逆时针方向，速度30%运行1.5秒"
    echo "  $0 stop       # 停止舵机"
    exit 1
}

# 主程序
init_pwm || exit 1

# 处理命令行参数
if [ $# -eq 0 ]; then
    usage
fi

case "$1" in
    "cw" | "ccw")
        # 验证速度
        if [ $# -lt 2 ] || ! [[ "$2" =~ ^[0-9]+$ ]] || [ "$2" -lt 0 ] || [ "$2" -gt 100 ]; then
            echo "错误：速度必须是 0 到 100 之间的整数！"
            usage
        fi

        # 验证时间（如果提供）
        if [ $# -ge 3 ] && (! [[ "$3" =~ ^[0-9]+(\.[0-9]+)?$ ]] || (($(echo "$3 <= 0" | bc -l)))); then
            echo "错误：时间必须是正数！"
            usage
        fi

        # 设置舵机
        set_servo "$1" "$2" "$3"
        ;;

    "stop")
        stop_servo
        ;;

    *)
        echo "错误：无效指令 '$1'！"
        usage
        ;;
esac

# 保持 PWM 启用状态（不自动禁用）
# 如果需要禁用，可以取消下面一行的注释
# echo 0 > "$PWM_PATH/enable"