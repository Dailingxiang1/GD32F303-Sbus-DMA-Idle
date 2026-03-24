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
uint8_t sbus_rx_buf[25] = 0; // S.BUS固定25字节
uint8_t sbus_flag = 0;   // 帧完成标志

void sbus_parse(uint8_t *buf) {
    if (buf[0] != 0x0F || buf[24] != 0x00) return; // 检查帧头帧尾

    // S.BUS 协议位操作解析 (11-bit 每个通道)
    sbus_data.ch[0]  = ((buf[1]    | buf[2] << 8)                          & 0x07FF);
    sbus_data.ch[1]  = ((buf[2] >> 3 | buf[3] << 5)                         & 0x07FF);
    sbus_data.ch[2]  = ((buf[3] >> 6 | buf[4] << 2 | buf[5] << 10)          & 0x07FF);
    sbus_data.ch[3]  = ((buf[5] >> 1 | buf[6] << 7)                         & 0x07FF);
    sbus_data.ch[4]  = ((buf[6] >> 4 | buf[7] << 4)                         & 0x07FF);
    sbus_data.ch[5]  = ((buf[7] >> 7 | buf[8] << 1 | buf[9] << 9)           & 0x07FF);
    sbus_data.ch[6]  = ((buf[9] >> 2 | buf[10] << 6)                        & 0x07FF);
    sbus_data.ch[7]  = ((buf[10] >> 5 | buf[11] << 3)                       & 0x07FF);
    sbus_data.ch[8]  = ((buf[12]   | buf[13] << 8)                         & 0x07FF);
    sbus_data.ch[9]  = ((buf[13] >> 3 | buf[14] << 5)                        & 0x07FF);
	
	// 解析第 24 字节 (buf[23])
    // Bit 3: Failsafe 激活标志
    // Bit 2: 帧丢失 (Frame Lost) 标志
    if (buf[23] & (1 << 3)) {
        sbus_data.failsafe = 1; // 遥控器没开或断连
    } else {
        sbus_data.failsafe = 0; // 信号正常
}
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

uint8_t rx_buffer[256];
uint8_t rx_index;

#define USART0_TX_PORT       GPIOB
#define USART0_TX_PIN GPIO_PIN_6
#define USART0_RX_PORT       GPIOB
#define USART0_RX_PIN GPIO_PIN_7

#define RCU_USART0_GPIO_PORT    RCU_GPIOB


#define UART3_RX_PIN GPIO_PIN_11
#define UART3_TX_PIN GPIO_PIN_10


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
}
static void usart3_config(void)
{
    rcu_periph_clock_enable(RCU_GPIOC);

	rcu_periph_clock_enable(RCU_UART3);

    gpio_init(GPIOC, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, UART3_TX_PIN);

	gpio_init(GPIOC, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, UART3_RX_PIN);

	usart_deinit(UART3);
    usart_baudrate_set(UART3, 100000UL);
	usart_parity_config(UART3, USART_PM_EVEN);
	usart_stop_bit_set(UART3, USART_STB_2BIT);
    usart_receive_config(UART3, USART_RECEIVE_ENABLE);
    usart_transmit_config(UART3, USART_TRANSMIT_ENABLE);
	
	/*串口中断配置*/
	usart_interrupt_enable(UART3, USART_INT_RBNE);
	
	nvic_irq_enable(UART3_IRQn, 0, 0);
	
    usart_enable(UART3);
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
    usart3_config();
    
    printf("S.BUS Parser Start...\r\n");

    while(1) {
        if(sbus_flag) {
            sbus_flag = 0;
            sbus_parse(sbus_rx_buf);

			if(sbus_data.failsafe == 0){
            // 按照你的通道要求打印
            printf("X2:%4d | Y2:%4d | Y1:%4d | X1:%4d | E:%4d | F:%4d | A:%4d | B:%4d | C:%4d | D:%4d\r\n",
                   sbus_data.ch[0], sbus_data.ch[1], sbus_data.ch[2], sbus_data.ch[3],
                   sbus_data.ch[4], sbus_data.ch[5], sbus_data.ch[6], sbus_data.ch[7],
                   sbus_data.ch[8], sbus_data.ch[9]);
			}
			else
				printf("No Sigal\r\n");
        }
        // S.BUS 周期通常是 7ms 或 14ms，这里不需要大的 delay
    }
}

void UART3_IRQHandler(void)                  
{
    static uint8_t count = 0;
    if(RESET != usart_interrupt_flag_get(UART3, USART_INT_FLAG_RBNE)){
        uint8_t res = usart_data_receive(UART3);
        
        // 简单的帧头对齐逻辑
        if (count == 0 && res != 0x0F) {
            return; 
        }

        sbus_rx_buf[count++] = res;

        if (count == 25) {
            count = 0;
            sbus_flag = 1; // 标记一帧接收完成
        }
    }
}
