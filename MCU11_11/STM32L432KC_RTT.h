// STM32L432KC_RTT.h
// RTT (Real-Time Transfer) debug output for STM32L432KC
//
// Uses SEGGER RTT for debug output through the debugger
// No UART pins needed - works through J-Link connection

#ifndef STM32L432KC_RTT_H
#define STM32L432KC_RTT_H

#include <stdint.h>
#include <stdbool.h>

// RTT buffer size
#define RTT_BUFFER_SIZE 1024

// Function prototypes
void RTT_Init(void);
void RTT_PrintChar(char c);
void RTT_PrintStr(const char *str);
void RTT_PrintInt(int32_t num);
void RTT_PrintFloat(float num, int decimals);
void RTT_PrintHex(uint32_t num);
void RTT_PrintNewline(void);

// Convenience macros (replacing UART macros)
#define DEBUG_PRINT(str) RTT_PrintStr(str)
#define DEBUG_PRINTLN(str) do { RTT_PrintStr(str); RTT_PrintNewline(); } while(0)
#define DEBUG_PRINT_NEWLINE() RTT_PrintNewline()
#define DEBUG_PRINT_INT(num) RTT_PrintInt(num)
#define DEBUG_PRINT_FLOAT(num, dec) RTT_PrintFloat(num, dec)
#define DEBUG_PRINT_HEX(num) RTT_PrintHex(num)

// Compatibility macros (for easy migration from UART)
#define UART_Init(baud) RTT_Init()
#define UART_PrintChar(c) RTT_PrintChar(c)
#define UART_PrintStr(s) RTT_PrintStr(s)
#define UART_PrintInt(n) RTT_PrintInt(n)
#define UART_PrintFloat(n, d) RTT_PrintFloat(n, d)
#define UART_PrintHex(n) RTT_PrintHex(n)
#define UART_PrintNewline() RTT_PrintNewline()

#endif // STM32L432KC_RTT_H

