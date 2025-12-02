// STM32L432KC_RTT.c
// RTT (Real-Time Transfer) debug output implementation for STM32L432KC
//
// Uses printf which is automatically redirected to RTT when LIBRARY_IO_TYPE="RTT"
// This works through the debugger connection - no UART pins needed

#include "STM32L432KC_RTT.h"
#include <stdio.h>
#include <stdint.h>

// Initialize RTT (no-op since printf is already redirected)
void RTT_Init(void) {
    // RTT is automatically initialized by SEGGER Embedded Studio
    // when LIBRARY_IO_TYPE="RTT" is set in project configuration
}

// Print a single character
void RTT_PrintChar(char c) {
    printf("%c", c);
}

// Print a string
void RTT_PrintStr(const char *str) {
    if (str != NULL) {
        printf("%s", str);
    }
}

// Print an integer
void RTT_PrintInt(int32_t num) {
    printf("%ld", (long)num);
}

// Print a float with specified decimal places
void RTT_PrintFloat(float num, int decimals) {
    // Build format string dynamically
    char format[8];
    snprintf(format, sizeof(format), "%%.%df", decimals);
    printf(format, num);
}

// Print a number in hexadecimal
void RTT_PrintHex(uint32_t num) {
    printf("0x%02lX", (unsigned long)num);  // Use %02lX for 2-digit hex (byte value)
}

// Print newline (CR + LF)
void RTT_PrintNewline(void) {
    RTT_PrintChar('\r');
    RTT_PrintChar('\n');
}

