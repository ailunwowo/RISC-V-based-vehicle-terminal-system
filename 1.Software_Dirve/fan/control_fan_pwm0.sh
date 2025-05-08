#!/bin/bash

# PWM 设备路径
PWM_PATH="/sys/class/pwm/pwmchip0/pwm0"

# 检查 PWM 路径是否存在
check_pwm() {
    if [ ! -d "$PWM_PATH" ]; then
        echo "错误：PWM 路径 $PWM_PATH 不存在！尝试导出 PWM 通道..."
        echo 0 > /sys/class/pwm/pwmchip0/export 2>/dev/null
        if [ ! -d "$PWM_PATH" ]; then
            echo "错误：无法导出 PWM0 通道！请检查设备或权限。"
            exit 1
        fi
    fi
}

# PWM 参数（默认 500Hz）
DEFAULT_FREQ=500
PERIOD=$((1000000000 / DEFAULT_FREQ))
MAX_DUTY=$PERIOD

# 初始化 PWM
init_pwm() {
    check_pwm
    
    # 禁用 PWM
    echo 0 > "$PWM_PATH/enable" 2>/dev/null

    # 设置周期
    echo $PERIOD > "$PWM_PATH/period" || {
        echo "错误：无法设置周期 $PERIOD 纳秒！"
        exit 1
    }

    # 设置初始占空比
    echo 0 > "$PWM_PATH/duty_cycle" || {
        echo "错误：无法设置占空比！"
        exit 1
    }

    # 设置极性
    echo "normal" > "$PWM_PATH/polarity" 2>/dev/null || 
    echo "inversed" > "$PWM_PATH/polarity" 2>/dev/null

    # 启用 PWM
    echo 1 > "$PWM_PATH/enable" || {
        echo "错误：无法启用 PWM！"
        exit 1
    }
}

# 设置风扇转速
set_speed() {
    local speed=$1
    local duty_cycle=$((speed * MAX_DUTY / 100))
    
    echo $duty_cycle > "$PWM_PATH/duty_cycle" || {
        echo "错误：无法设置占空比 $duty_cycle 纳秒！"
        exit 1
    }
    echo "风扇转速设置为 $speed%（占空比 $duty_cycle 纳秒）"
}

# 使用说明
usage() {
    echo "用法：$0 [选项]"
    echo "选项："
    echo "  -s, --speed <0-100>   设置风扇转速百分比"
    echo "  -f, --freq <Hz>       设置 PWM 频率（默认500Hz）"
    echo "  -S, --stop            停止风扇"
    echo "  -h, --help            显示帮助信息"
    echo
    echo "示例："
    echo "  $0 -f 250 -s 50      设置500Hz频率并将转速设为50%"
    echo "  $0 --stop            完全停止风扇"
}

# 主程序
main() {
    local speed=-1
    local freq=$DEFAULT_FREQ
    local stop_flag=0

    # 解析参数
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -s|--speed)
                if [[ "$2" =~ ^[0-9]+$ ]] && (( "$2" >= 0 && "$2" <= 100 )); then
                    speed=$2
                    shift 2
                else
                    echo "错误：无效的转速值 '$2'，必须是0-100的整数"
                    exit 1
                fi
                ;;
            -f|--freq)
                if [[ "$2" =~ ^[0-9]+$ ]] && (( "$2" >= 10 && "$2" <= 10000 )); then
                    freq=$2
                    shift 2
                else
                    echo "错误：无效的频率值 '$2'，必须是10-10000的整数"
                    exit 1
                fi
                ;;
            -S|--stop)
                stop_flag=1
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                echo "错误：未知选项 '$1'"
                usage
                exit 1
                ;;
        esac
    done

    # 初始化硬件
    init_pwm

    # 设置频率
    if (( freq != DEFAULT_FREQ )); then
        PERIOD=$((1000000000 / freq))
        MAX_DUTY=$PERIOD
        echo $PERIOD > "$PWM_PATH/period" || {
            echo "错误：无法设置频率 $freq Hz（周期 $PERIOD 纳秒）！"
            exit 1
        }
        echo "PWM 频率设置为 $freq Hz"
    fi

    # 执行操作
    if (( stop_flag == 1 )); then
        echo 0 > "$PWM_PATH/duty_cycle"
        echo "风扇已停止"
    elif (( speed != -1 )); then
        set_speed $speed
    else
        echo "错误：未指定操作参数"
        usage
        exit 1
    fi

    # 保持运行（防止立即退出导致PWM禁用）
    if (( stop_flag == 0 )); then
        echo "按CTRL+C退出并重置PWM..."
        trap 'echo 0 > "$PWM_PATH/enable"; exit' SIGINT
        while true; do sleep 1; done
    fi
}

# 启动主程序
main "$@"