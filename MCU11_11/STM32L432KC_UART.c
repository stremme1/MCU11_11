// STM32L432KC_UART.c
// UART debug output implementation for STM32L432KC

#include "STM32L432KC_UART.h"
#include "STM32L432KC_RCC.h"
#include "STM32L432KC_TIMER.h"  // Provides GPIOA and GPIO_TypeDef
#include <stdint.h>

// Initialize USART2 for debug output
// PA2 = TX, PA3 = RX
// Default: 115200 baud
void UART_Init(uint32_t baudrate) {
    // Enable GPIOA and USART2 clocks
    RCC->AHB2ENR |= (1 << 0);  // GPIOA
    RCC->APB1ENR1 |= (1 << 17); // USART2 (bit 17 of APB1ENR1)
    
    volatile int delay = 10;
    while (delay-- > 0) {
        __asm("nop");
    }
    
    // Configure PA2 (TX) - Alternate function mode, AF7 for USART2_TX
    GPIOA->MODER &= ~(0b11 << (2 * 2));
    GPIOA->MODER |= (0b10 << (2 * 2));  // Alternate function
    GPIOA->AFRL &= ~(0b1111 << (4 * 2));
    GPIOA->AFRL |= (0b0111 << (4 * 2));  // AF7 = USART2_TX
    GPIOA->OSPEEDR |= (0b11 << (2 * 2));  // High speed
    
    // Configure PA3 (RX) - Alternate function mode, AF7 for USART2_RX
    GPIOA->MODER &= ~(0b11 << (2 * 3));
    GPIOA->MODER |= (0b10 << (2 * 3));  // Alternate function
    GPIOA->AFRL &= ~(0b1111 << (4 * 3));
    GPIOA->AFRL |= (0b0111 << (4 * 3));  // AF7 = USART2_RX
    GPIOA->PURPDR |= (0b01 << (2 * 3));  // Pull-up
    
    // Reset USART2
    RCC->APB1RSTR1 |= (1 << 17);
    delay = 10;
    while (delay-- > 0) {
        __asm("nop");
    }
    RCC->APB1RSTR1 &= ~(1 << 17);
    
    // Configure USART2
    // BRR calculation: BRR = fCK / baudrate
    // For 80MHz PCLK and 115200 baud: BRR = 80000000 / 115200 = 694.44
    // BRR = 0x02B6 (mantissa = 0x2B6 = 694, fraction = 0x6 = 0.375)
    uint32_t pclk = 80000000;  // APB1 clock (80MHz)
    uint32_t brr_value = pclk / baudrate;
    USART2->BRR = brr_value;
    
    // Enable USART2: TE (TX enable), RE (RX enable), UE (USART enable)
    USART2->CR1 = 0;
    USART2->CR1 |= (1 << 0);   // UE - USART enable
    USART2->CR1 |= (1 << 2);   // RE - Receiver enable
    USART2->CR1 |= (1 << 3);   // TE - Transmitter enable
}

// Print a single character
void UART_PrintChar(char c) {
    // Wait until transmit data register is empty
    while (!(USART2->ISR & (1 << 7))) {  // TXE bit
        __asm("nop");
    }
    
    // Write character to transmit data register
    USART2->TDR = (uint16_t)c;
}

// Print a string
void UART_PrintStr(const char *str) {
    while (*str) {
        UART_PrintChar(*str++);
    }
}

// Print an integer
void UART_PrintInt(int32_t num) {
    char buffer[12];
    int i = 0;
    bool negative = false;
    
    if (num < 0) {
        negative = true;
        num = -num;
    }
    
    if (num == 0) {
        buffer[i++] = '0';
    } else {
        while (num > 0 && i < 11) {
            buffer[i++] = '0' + (num % 10);
            num /= 10;
        }
    }
    
    if (negative) {
        buffer[i++] = '-';
    }
    
    // Reverse the string
    for (int j = 0; j < i / 2; j++) {
        char temp = buffer[j];
        buffer[j] = buffer[i - 1 - j];
        buffer[i - 1 - j] = temp;
    }
    
    buffer[i] = '\0';
    UART_PrintStr(buffer);
}

// Print a float with specified decimal places
void UART_PrintFloat(float num, int decimals) {
    if (num < 0) {
        UART_PrintChar('-');
        num = -num;
    }
    
    // Print integer part
    int32_t int_part = (int32_t)num;
    UART_PrintInt(int_part);
    
    if (decimals > 0) {
        UART_PrintChar('.');
        
        // Print decimal part
        float frac = num - (float)int_part;
        for (int i = 0; i < decimals; i++) {
            frac *= 10.0f;
            int digit = (int)frac % 10;
            UART_PrintChar('0' + digit);
        }
    }
}

// Print a number in hexadecimal
void UART_PrintHex(uint32_t num) {
    UART_PrintStr("0x");
    
    if (num == 0) {
        UART_PrintChar('0');
        return;
    }
    
    char buffer[9];
    int i = 0;
    
    while (num > 0 && i < 8) {
        uint32_t digit = num & 0xF;
        if (digit < 10) {
            buffer[i++] = '0' + digit;
        } else {
            buffer[i++] = 'A' + (digit - 10);
        }
        num >>= 4;
    }
    
    // Reverse and print
    for (int j = i - 1; j >= 0; j--) {
        UART_PrintChar(buffer[j]);
    }
}

// Print newline (CR + LF)
void UART_PrintNewline(void) {
    UART_PrintChar('\r');
    UART_PrintChar('\n');
}

