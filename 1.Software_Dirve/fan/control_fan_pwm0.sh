#!/bin/bash

# PWM 设备路径
PWM_PATH="/sys/class/pwm/pwmchip0/pwm0"

# 检查 PWM 路径是否存在
if [ ! -d "$PWM_PATH" ]; then
    echo "错误：PWM 路径 $PWM_PATH 不存在！尝试导出 PWM 通道..."
    echo 0 > /sys/class/pwm/pwmchip0/export 2>/dev/null
    if [ ! -d "$PWM_PATH" ]; then
        echo "错误：无法导出 PWM0 通道！请检查设备或权限。"
        exit 1
    fi
fi

# PWM 参数（使用 500Hz，2ms）
PERIOD=2000000  # 2ms (500Hz)
MAX_DUTY=2000000 # 100% 占空比 (等于周期)

# 初始化 PWM
init_pwm() {
    # 禁用 PWM
    echo 0 > "$PWM_PATH/enable" 2>/dev/null

    # 设置周期
    echo $PERIOD > "$PWM_PATH/period" || {
        echo "错误：无法设置周期 $PERIOD 纳秒！请检查硬件支持的周期范围。"
        return 1
    }

    # 设置初始占空比（停止）
    echo 0 > "$PWM_PATH/duty_cycle" || {
        echo "错误：无法设置占空比！"
        return 1
    }

    # 设置极性
    echo "normal" > "$PWM_PATH/polarity" || {
        echo "错误：无法设置极性！尝试使用 inversed。"
        echo "inversed" > "$PWM_PATH/polarity" || return 1
    }

    # 启用 PWM
    echo 1 > "$PWM_PATH/enable" || {
        echo "错误：无法启用 PWM！"
        return 1
    }

    echo "PWM 初始化成功，周期 $PERIOD 纳秒（500Hz），风扇已停止。"
    return 0
}

# 设置风扇转速
set_fan_speed() {
    local speed=$1
    local duty_cycle=$((speed * MAX_DUTY / 100))

    # 设置占空比
    echo $duty_cycle > "$PWM_PATH/duty_cycle" || {
        echo "错误：无法设置占空比 $duty_cycle 纳秒！"
        return 1
    }

    echo "成功：风扇转速设置为 $speed%，占空比 $duty_cycle 纳秒。"
    return 0
}

# 停止风扇
stop_fan() {
    echo 0 > "$PWM_PATH/duty_cycle" || {
        echo "错误：无法停止风扇！"
        return 1
    }
    echo "风扇已停止（占空比设置为 0 纳秒）。"
    return 0
}

# 设置 PWM 频率
set_frequency() {
    local freq=$1
    local period=$((1000000000 / freq)) # 周期（纳秒）= 10^9 / 频率（Hz）

    # 禁用 PWM
    echo 0 > "$PWM_PATH/enable" 2>/dev/null

    # 设置新周期
    echo $period > "$PWM_PATH/period" || {
        echo "错误：无法设置频率 $freq Hz（周期 $period 纳秒）！"
        return 1
    }

    # 更新全局变量
    PERIOD=$period
    MAX_DUTY=$period

    # 重新启用 PWM
    echo 0 > "$PWM_PATH/duty_cycle"
    echo 1 > "$PWM_PATH/enable" || {
        echo "错误：无法重新启用 PWM！"
        return 1
    }

    echo "成功：PWM 频率设置为 $freq Hz（周期 $period 纳秒）。"
    return 0
}

# 使用说明
usage() {
    echo "指令格式："
    echo "  speed <百分比>    # 设置风扇转速，0-100"
    echo "  freq <频率>       # 设置 PWM 频率（Hz），例如 500 表示 500Hz"
    echo "  stop             # 停止风扇"
    echo "  exit 或 quit     # 退出脚本"
    echo "注意：硬件支持的频率范围有限，建议尝试 25Hz、250Hz、500Hz（周期 40000000、4000000、2000000 纳秒）。"
    echo "示例："
    echo "  freq 500         # 设置 PWM 频率为 500Hz"
    echo "  speed 50         # 设置风扇转速为 50%"
    echo "  stop             # 停止风扇"
}

# 初始化 PWM
init_pwm || exit 1

# 进入循环，等待用户输入
echo "进入循环测试模式，输入指令控制 PWM0 风扇（输入 'exit' 或 'quit' 退出）："
while true; do
    echo -n "> "
    read -r command arg1

    # 处理输入
    case "$command" in
        "speed")
            # 验证转速
            if ! [[ "$arg1" =~ ^[0-9]+$ ]] || [ "$arg1" -lt 0 ] || [ "$arg1" -gt 100 ]; then
                echo "错误：转速必须是 0 到 100 之间的整数！"
                usage
                continue
            fi

            # 设置转速
            set_fan_speed "$arg1"
            ;;

        "freq")
            # 验证频率
            if ! [[ "$arg1" =~ ^[0-9]+$ ]] || [ "$arg1" -lt 10 ] || [ "$arg1" -gt 10000 ]; then
                echo "错误：频率必须是 10 到 10000 Hz 之间的整数！"
                usage
                continue
            fi

            # 设置频率
            set_frequency "$arg1"
            ;;

        "stop")
            stop_fan
            ;;

        "exit" | "quit")
            stop_fan
            echo 0 > "$PWM_PATH/enable"
            echo "退出脚本，PWM 已禁用。"
            exit 0
            ;;

        *)
            echo "错误：无效指令 '$command'！"
            usage
            ;;
    esac
done
