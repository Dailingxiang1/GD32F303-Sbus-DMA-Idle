#include "gd32f30x.h"
#include "systick.h"

// 定义 APP 起始地址
#define APP_START_ADDR    0x08008000

typedef void (*app_entry_t)(void); // 定义函数指针类型

void bootloader_jump_to_app(void)
{
    uint32_t stack_pointer = *(__IO uint32_t *)APP_START_ADDR;
    uint32_t jump_address = *(__IO uint32_t *)(APP_START_ADDR + 4);
    app_entry_t app_reset_handler;

    /* 1. 检查栈顶地址是否合法 (通常 GD32F303 的 RAM 范围在 0x20000000 以后) */
    if ((stack_pointer & 0x2FFE0000) == 0x20000000)
    {
        /* 2. 彻底关闭所有外设，特别是中断 */
        __disable_irq(); 
		
		usart_deinit(USART0);
        
        /* 3. 关闭 SysTick 定时器并重置其状态 */
        SysTick->CTRL = 0;
        SysTick->LOAD = 0;
        SysTick->VAL = 0;

        /* 4. 清除所有待处理的中断标志 (NVIC Clear Pending) */
        for (int i = 0; i < 8; i++) {
            NVIC->ICPR[i] = 0xFFFFFFFF;
        }

        /* 5. 设置主堆栈指针 (MSP) */
        __set_MSP(stack_pointer);

        /* 6. 获取 APP 的复位向量并跳转 */
        app_reset_handler = (app_entry_t)jump_address;
        app_reset_handler();
    }
    else
    {
        /* 如果栈顶地址非法，说明 APP 区域可能没有有效的程序 */
        // 处理错误：比如闪烁 LED 或停留在此处
    }
}

#define USART0_TX_PORT       GPIOB
#define USART0_TX_PIN GPIO_PIN_6
#define USART0_RX_PORT       GPIOB
#define USART0_RX_PIN GPIO_PIN_7

#define RCU_USART0_GPIO_PORT    RCU_GPIOB

static void usart0_config(void)
{
    rcu_periph_clock_enable(RCU_USART0_GPIO_PORT);

	rcu_periph_clock_enable(RCU_USART0);

	rcu_periph_clock_enable(RCU_AF);
	
	gpio_pin_remap_config(GPIO_USART0_REMAP, ENABLE);
	
    gpio_init(USART0_TX_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, USART0_TX_PIN);

	gpio_init(USART0_RX_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, USART0_RX_PIN);

	
	
	usart_deinit(USART0);
    usart_baudrate_set(USART0, 115200UL);

    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
	
	usart_enable(USART0);
//    usart_dma_receive_config(USART0, USART_RECEIVE_DMA_ENABLE);

}

void uart_sendstring(uint8_t* str)
{
	while (*str != '\0') {  // 只判断，不移动
        usart_data_transmit(USART0, (uint8_t)*str);
        
        // 等待发送缓冲区空 (TBE) 比等待发送完成 (TC) 效率更高
        while (usart_flag_get(USART0, USART_FLAG_TBE) == RESET);
        
        str++;  // 发送完一个，再移动到下一个
    }
}
/*!
    \brief      main function
    \param[in]  none
    \param[out] none
    \retval     none
*/

int main(void)
{	
    systick_config();
	usart0_config();
	
	uart_sendstring((uint8_t*)"Ready to Jump To APP at 0x08008000\r\n");
	while (usart_flag_get(USART0, USART_FLAG_TC) == RESET);
	
	nvic_vector_table_set(NVIC_VECTTAB_FLASH, 0x8000);
	bootloader_jump_to_app();
	while(1) {

		}
}
