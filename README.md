# Key Features / 核心特性
- DMA Circular Mode: 自动循环搬运，无需手动干预。
- IDLE Interrupt Alignment: 通过空闲中断检测 dma_transfer_number_get()，一旦字节数不对齐（Phase Shift），立即强制重置 DMA 通道，实现毫秒级自愈。
- Low Latency: 硬件级搬运，解析逻辑仅在每帧结束时运行一次。
# Hardware Connection / 硬件连接
- UART: UART3 (PC11-RX)
- Signal: S.BUS Inverted (需要硬件反相器或确认接收器支持非反相输出)
- Baudrate: 100,000 bps, 8E2 (8位数据, 偶校验, 2停止位)

# 通道含义
- Channel_1:  X2
- Channel_2:  Y2
- Channel_3:  Y1
- Channel_4:  X1
- Channel_5:  E
- Channel_6:  F
- Channel_7:  A
- Channel_8:  B
- Channel_9:  C
- Channel_10: D

# ATTENTION
- 遥控器和接收器分别为SKYROID(云卓科技的) T10 和 R10