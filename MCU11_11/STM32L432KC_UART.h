// STM32L432KC_UART.h
// UART debug output for STM32L432KC
//
// Provides simple UART output for debugging via USART2
// TX: PA2, RX: PA3 (standard STM32 debug UART)

#ifndef STM32L432KC_UART_H
#define STM32L432KC_UART_H

#include <stdint.h>
#include <stdbool.h>

// USART2 base address
#define USART2_BASE (0x40004400UL)

// USART register structure
typedef struct {
    volatile uint32_t CR1;      // Control register 1
    volatile uint32_t CR2;       // Control register 2
    volatile uint32_t CR3;      // Control register 3
    volatile uint32_t BRR;      // Baud rate register
    volatile uint32_t GTPR;     // Guard time and prescaler
    volatile uint32_t RTOR;     // Receiver timeout
    volatile uint32_t RQR;      // Request register
    volatile uint32_t ISR;      // Interrupt and status register
    volatile uint32_t ICR;      // Interrupt flag clear register
    volatile uint32_t RDR;      // Receive data register
    volatile uint32_t TDR;      // Transmit data register
    volatile uint32_t PRESC;    // Prescaler register
} USART_TypeDef;

#define USART2 ((USART_TypeDef *) USART2_BASE)

// Function prototypes
void UART_Init(uint32_t baudrate);
void UART_PrintChar(char c);
void UART_PrintStr(const char *str);
void UART_PrintInt(int32_t num);
void UART_PrintFloat(float num, int decimals);
void UART_PrintHex(uint32_t num);
void UART_PrintNewline(void);

// Convenience macros
#define DEBUG_PRINT(str) UART_PrintStr(str)
#define DEBUG_PRINTLN(str) do { UART_PrintStr(str); UART_PrintNewline(); } while(0)
#define DEBUG_PRINT_NEWLINE() UART_PrintNewline()
#define DEBUG_PRINT_INT(num) UART_PrintInt(num)
#define DEBUG_PRINT_FLOAT(num, dec) UART_PrintFloat(num, dec)
#define DEBUG_PRINT_HEX(num) UART_PrintHex(num)

#endif // STM32L432KC_UART_H

