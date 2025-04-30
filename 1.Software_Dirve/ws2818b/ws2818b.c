#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/ktime.h>

#define GPIO_NUM 50  // 使用GPIO50

/******************** 精准延时实现 ********************/
static inline void delay_cycles(unsigned long cycles)
{
    ktime_t start = ktime_get_ns();
    while ((ktime_get_ns() - start) < cycles);
}

// 校准后的延时函数（根据实际CPU频率调整）
#define CPU_FREQ 1000000000  // 假设CPU 1GHz，需根据实际情况修改

static void delay_300ns(void) {
    delay_cycles(CPU_FREQ * 300 / 1000000000); // 300ns
}

static void delay_1090ns(void) {
    delay_cycles(CPU_FREQ * 1090 / 1000000000); // 1090ns
}

/******************** 与STM32完全相同的函数实现 ********************/
static void RGB_Init(void) {
    gpio_request(GPIO_NUM, "WS2812B");
    gpio_direction_output(GPIO_NUM, 0);
}

static void RGB_Reset(void) {
    gpio_set_value(GPIO_NUM, 0);
    udelay(350);
}

static void RGB_Send0(void) {
    gpio_set_value(GPIO_NUM, 1);
    delay_300ns();
    gpio_set_value(GPIO_NUM, 0);
    delay_1090ns();
}

static void RGB_Send1(void) {
    gpio_set_value(GPIO_NUM, 1);
    delay_1090ns();
    gpio_set_value(GPIO_NUM, 0);
    delay_300ns();
}

static void RGB_Send_Data(u8 data) { 
    int i;
    for(i=8; i>0; i--) {
        if(data & 0x80) RGB_Send1();
        else RGB_Send0();
        data <<= 1;
    }
}

static void Send_GRB(uint8_t G, uint8_t R, uint8_t B) {
    RGB_Send_Data(G);
    RGB_Send_Data(R);
    RGB_Send_Data(B);
    RGB_Reset();
}

static void Continuous_Set_LED(uint8_t n, uint32_t GRB) {
    while(n--) {
        RGB_Send_Data((GRB>>16)&0xFF);
        RGB_Send_Data((GRB>>8)&0xFF);
        RGB_Send_Data(GRB&0xFF);
    }
    RGB_Reset();
}

/******************** 关键优化措施 ********************/
static void __send_data_safe(void) {
    unsigned long flags;
    
    // 禁用中断保证时序
    local_irq_save(flags);
    
    // 发送数据（示例：点亮10个红灯）
    Continuous_Set_LED(10, 0xFF0000);
    
    // 恢复中断
    local_irq_restore(flags);
}

/******************** 模块初始化 ********************/
static int __init ws2812b_init(void) {
    RGB_Init();
    __send_data_safe();
    return 0;
}

static void __exit ws2812b_exit(void) {
    Continuous_Set_LED(10, 0x000000); // 关闭所有灯
    gpio_free(GPIO_NUM);
}

module_init(ws2812b_init);
module_exit(ws2812b_exit);
MODULE_LICENSE("GPL");