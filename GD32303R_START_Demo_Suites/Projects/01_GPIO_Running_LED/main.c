/*!
    \file    main.c
    \brief   systick LED demo

    \version 2025-08-20, V3.0.2, demo for GD32F30x
*/

/*
    Copyright (c) 2025, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

#include "gd32f30x.h"
#include "gd32f303r_start.h"
#include "systick.h"

#include <stdio.h>

int fputc(int Byte, FILE* ftr)
{
	usart_data_transmit(USART0, Byte);
	while(usart_flag_get(USART0, USART_FLAG_TC) == RESET);
	return Byte;
}

typedef struct {
    uint16_t ch[10];
    uint8_t  failsafe; // 0: 正常, 1: 掉电/失控
} Sbus_Channels_t;

Sbus_Channels_t sbus_data;
uint8_t sbus_rx_buf[25]; // S.BUS固定25字节
uint8_t sbus_flag = 0;   // 帧完成标志

// 修改返回值为 uint8_t (1:成功, 0:失败)
uint8_t sbus_parse(volatile uint8_t *buf) {
    /* S.BUS 标准：帧头 0x0F, 帧尾 0x00 */
    /* 某些接收器在高速模式下帧尾可能是 0x04, 0x14 等，通常 0x00 最通用 */
    if (buf[0] != 0x0F) {
        return 0; 
    }
    
    // 如果你发现数据是对的但帧尾不对，可以临时把 buf[24] != 0x00 去掉调试
    if (buf[24] != 0x00) {
        // return 0; 
    }

    // 解析 11-bit 通道 (1-10通道)
    sbus_data.ch[0] = ((buf[1]       | buf[2] << 8) & 0x07FF);
    sbus_data.ch[1] = ((buf[2] >> 3  | buf[3] << 5) & 0x07FF);
    sbus_data.ch[2] = ((buf[3] >> 6  | buf[4] << 2 | buf[5] << 10) & 0x07FF);
    sbus_data.ch[3] = ((buf[5] >> 1  | buf[6] << 7) & 0x07FF);
    sbus_data.ch[4] = ((buf[6] >> 4  | buf[7] << 4) & 0x07FF);
    sbus_data.ch[5] = ((buf[7] >> 7  | buf[8] << 1 | buf[9] << 9) & 0x07FF);
    sbus_data.ch[6] = ((buf[9] >> 2  | buf[10] << 6) & 0x07FF);
    sbus_data.ch[7] = ((buf[10] >> 5 | buf[11] << 3) & 0x07FF);
    sbus_data.ch[8] = ((buf[12]      | buf[13] << 8) & 0x07FF);
    sbus_data.ch[9] = ((buf[13] >> 3 | buf[14] << 5) & 0x07FF);

    // 解析第 24 字节：Failsafe 标志
    // Bit 3: Failsafe active, Bit 2: Frame lost
    sbus_data.failsafe = (buf[23] & 0x08) ? 1 : 0;

    return 1;
}
/*
PA9  --> USART0_TX
PA10 --> USART0_RX

PC11 --> UART3_RX
PC10 --> UART3_TX
*/

/*
Channel_1:  X2
Channel_2:  Y2
Channel_3:  Y1
Channel_4:  X1
Channel_5:  E
Channel_6:  F
Channel_7:  A
Channel_8:  B
Channel_9:  C
Channel_10: D
*/

#define USART0_TX_PORT       GPIOB
#define USART0_TX_PIN GPIO_PIN_6
#define USART0_RX_PORT       GPIOB
#define USART0_RX_PIN GPIO_PIN_7

#define RCU_USART0_GPIO_PORT    RCU_GPIOB


#define UART3_RX_PIN GPIO_PIN_11
#define UART3_TX_PIN GPIO_PIN_10

uint8_t usart0_rx_dma[25];
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
	
    usart_dma_receive_config(USART0, USART_RECEIVE_DMA_ENABLE);

}

void usart0_dma_config(void)
{
    dma_parameter_struct dma_init_struct;
    
    /* 开启 DMA0 时钟 (USART0 对应 DMA0 Channel 4) */
    rcu_periph_clock_enable(RCU_DMA0);

    /* 清除之前的配置 */
    //dma_deinit(DMA0, DMA_CH4);
	//usart_dma_receive_config(USART0, USART_RECEIVE_DMA_ENABLE);
	usart_flag_clear(USART0, USART_FLAG_RBNE);
	
    dma_struct_para_init(&dma_init_struct);
    dma_init_struct.periph_addr  = (uint32_t)&USART_DATA(USART0); // 串口数据寄存器地址
    dma_init_struct.memory_addr  = (uint32_t)usart0_rx_dma;      // 内存目标地址
    dma_init_struct.direction    = DMA_PERIPHERAL_TO_MEMORY;   // 外设到内存
    dma_init_struct.number       = 25;                         // 传输长度
    dma_init_struct.periph_inc   = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc   = DMA_MEMORY_INCREASE_ENABLE; // 内存地址自增
    dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma_init_struct.priority     = DMA_PRIORITY_ULTRA_HIGH;
    dma_init(DMA0, DMA_CH4, &dma_init_struct);
	
	dma_circulation_disable(DMA0, DMA_CH4); 
	dma_channel_enable(DMA0, DMA_CH4);
}


static void usart3_config(void) {
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_UART3);
	
    gpio_init(GPIOC, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, UART3_TX_PIN);
    gpio_init(GPIOC, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, UART3_RX_PIN);

    // 1. 先 Deinit
    usart_deinit(UART3);
    
    // 2. 配置参数
    usart_baudrate_set(UART3, 100000UL);
    usart_parity_config(UART3, USART_PM_EVEN);
    usart_stop_bit_set(UART3, USART_STB_2BIT);
    usart_receive_config(UART3, USART_RECEIVE_ENABLE);
    
    // 3. 使能串口
    //usart_enable(UART3);
    
    // 4. 【关键】在串口使能后，强行开启 DMA 接收请求和空闲中断
    USART_CTL2(UART3) |= (uint32_t)0x40;  // 强制写 DENR 位
    usart_interrupt_enable(UART3, USART_INT_IDLE);
    nvic_irq_enable(UART3_IRQn, 0, 0);
}

/* 1. 定义一个缓冲区 */
volatile uint8_t sbus_rx_dma[25];

void usart3_dma_config(void) {
    dma_parameter_struct dma_init_struct;
    rcu_periph_clock_enable(RCU_DMA1);
    dma_deinit(DMA1, DMA_CH2);
    
    dma_struct_para_init(&dma_init_struct);
    dma_init_struct.periph_addr  = (uint32_t)&USART_DATA(UART3);
    dma_init_struct.memory_addr  = (uint32_t)sbus_rx_dma;
    dma_init_struct.direction    = DMA_PERIPHERAL_TO_MEMORY;
    dma_init_struct.number       = 25;
    dma_init_struct.periph_inc   = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc   = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma_init_struct.priority     = DMA_PRIORITY_ULTRA_HIGH;
    dma_init(DMA1, DMA_CH2, &dma_init_struct);

    dma_circulation_enable(DMA1, DMA_CH2);
    dma_channel_enable(DMA1, DMA_CH2);
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
	usart0_dma_config();
    usart0_config();
	usart3_dma_config(); 
    usart3_config();
	
	usart_enable(USART0);
	usart_enable(UART3);
    printf("S.BUS Parser Start...\r\n");

	while(1) {
			if(sbus_flag) {
				sbus_flag = 0; // 抢占标志位
				
				if(sbus_parse(sbus_rx_dma)) {
					if(sbus_data.failsafe) {
						printf("[WARN] Failsafe Active!\r\n");
					} else {
						// 打印关键通道数据，%4d 保持排版整齐
						printf("CH1:%4d CH2:%4d CH3:%4d CH4:%4d | S1:%4d S2:%4d\r\n",
							   sbus_data.ch[0], sbus_data.ch[1], sbus_data.ch[2], 
							   sbus_data.ch[3], sbus_data.ch[4], sbus_data.ch[5]);
					}
				} else {
					// 此时通常是发生了相位偏移，中断里的重置逻辑会在下一帧修复它
					 printf("Syncing...\r\n"); 
				}
			}
			
			// 如果需要低频打印状态，可以用 systick 计时，千万别用 delay_1ms
		}
}

void UART3_IRQHandler(void) {
    if (RESET != usart_interrupt_flag_get(UART3, USART_INT_FLAG_IDLE)) {
        /* 1. 硬件要求：清除 IDLE 标志位的标准序列 */
        usart_interrupt_flag_get(UART3, USART_INT_FLAG_IDLE);
        (void)usart_data_receive(UART3); 

        /* 2. 检查对齐：DMA 是向下计数的 */
        uint32_t remaining = dma_transfer_number_get(DMA1, DMA_CH2);
        
        /* 理想状态：一帧 25 字节正好收完，remaining 应该是 0 (然后自动重装为 25) 
           或者刚开始收下一帧。如果偏离太远，说明对齐丢了 */
        if (remaining != 25 && remaining != 0) {
            /* 发现相位偏移：强制停掉 DMA，重置计数器到 25，重新开始 */
            dma_channel_disable(DMA1, DMA_CH2);
            dma_transfer_number_config(DMA1, DMA_CH2, 25);
            dma_channel_enable(DMA1, DMA_CH2);
            
            sbus_flag = 0; // 丢弃当前这帧乱掉的数据
        } else {
            /* 状态正确，通知 Main 循环去解析 sbus_rx_dma[0..24] */
            sbus_flag = 1; 
        }
    }
}