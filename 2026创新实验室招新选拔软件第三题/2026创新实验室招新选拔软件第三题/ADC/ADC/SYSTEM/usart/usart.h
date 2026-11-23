#ifndef __USART_H
#define __USART_H
#include "stdio.h"
#include "stm32f4xx_conf.h"
#include "sys.h"

#define USART_REC_LEN       200
#define EN_USART1_RX        1

/* USART2 TX DMA buffer: 2048 lines of up to 6 characters. */
#define UART2_TX_BUF_SIZE   12288U

extern u8  USART_RX_BUF[USART_REC_LEN];
extern u16 USART_RX_STA;

extern char g_uart2_tx_buf[UART2_TX_BUF_SIZE];

void uart_init(u32 bound);
void uart2_init(u32 bound);
u8   uart2_tx_busy(void);
u8   uart2_send_dma(u16 len);
u8   uart2_getc(u8 *c);
u8   uart2_rx_overflow(void);

#endif
