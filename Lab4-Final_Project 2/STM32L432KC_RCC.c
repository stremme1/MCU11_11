// STM32L432KC_RCC.c
// Source code for RCC functions

#include "STM32L432KC_RCC.h"

void configurePLL() {
    // Set clock to 80 MHz
    // PLL Formula: SYSCLK = (MSI / PLLM) * PLLN / PLLR
    // Where:
    //   - MSI = 4 MHz (internal oscillator)
    //   - PLLM = 1 (division factor, register value 0)
    //   - PLLN = 80 (multiplication factor)
    //   - PLLR = 4 (division factor, register value 1)
    // Calculation: (4 MHz / 1) * 80 / 4 = 320 MHz / 4 = 80 MHz
    // Use MSI as PLLSRC

    // Turn off PLL
    RCC->CR &= ~(1 << 24);
    
    // Wait till PLL is unlocked (e.g., off)
    while ((RCC->CR >> 25 & 1) != 0);

    // Load configuration
    // Set PLL SRC to MSI
    RCC->PLLCFGR |= (1 << 0);
    RCC->PLLCFGR &= ~(1 << 1);

    // Set PLLN (bits 8-14, 7 bits total)
    // PLLN = 80 (0b1010000 in binary)
    // Formula: PLLN = (value in bits 8-14) directly
    RCC->PLLCFGR &= ~(0x7F << 8);  // Clear PLLN bits (bits 8-14, mask 0x7F00)
    RCC->PLLCFGR |= (80 << 8);      // Set PLLN = 80
    
    // Set PLLM (bits 4-6, 3 bits total)
    // PLLM = ((value in bits 4-6) >> 4) + 1
    // For PLLM = 1: register value must be 0 (since (0 >> 4) + 1 = 1)
    RCC->PLLCFGR &= ~(0x7 << 4);   // Clear PLLM bits (bits 4-6, mask 0x70)
    // PLLM = 0 means actual PLLM = 0 + 1 = 1 ✓
    
    // Set PLLR (bits 25-26, 2 bits total)
    // PLLR = (((value in bits 25-26) >> 25) + 1) * 2
    // For PLLR = 4: we need ((value >> 25) + 1) * 2 = 4
    // So: (value >> 25) + 1 = 2, therefore value >> 25 = 1
    // This means bit 25 = 1, bit 26 = 0 (value = 1)
    RCC->PLLCFGR &= ~(0x3 << 25);  // Clear PLLR bits (bits 25-26, mask 0x6000000)
    RCC->PLLCFGR |= (0x1 << 25);    // Set bit 25 = 1, bit 26 = 0 (value = 1)
    // PLLR = (1 + 1) * 2 = 4 ✓
    
    // Enable PLLR output
    RCC->PLLCFGR |= (1 << 24);

    // Enable PLL
    RCC->CR |= (1 << 24);
    
    // Wait until PLL is locked
    while ((RCC->CR >> 25 & 1) != 1);
}

void configureClock(){
    // CRITICAL: Set prescalers to DIV1 BEFORE switching to PLL
    // This ensures no clock division when we switch
    // HPRE (AHB prescaler, bits 4-7): Set to DIV1 (0b0000) so HCLK = SYSCLK
    // 0b0000 = DIV1, 0b1000 = DIV2, etc.
    RCC->CFGR &= ~(0xF << 4);  // Clear HPRE bits (bits 4-7) = 0b0000 = DIV1
    
    // PPRE1 (APB1 prescaler, bits 8-10): Set to DIV1 (0b000) so APB1 = HCLK
    // APB1 is used by DAC, so this is critical!
    // Encoding from datasheet: 0b000 = DIV1, 0b100 = DIV2, 0b101 = DIV4, 0b110 = DIV8, 0b111 = DIV16
    // Values 0b001, 0b010, 0b011 are reserved
    // Explicitly write 0 to bits 8-10 to ensure DIV1
    RCC->CFGR &= ~(0x7 << 8);  // Clear PPRE1 bits (bits 8-10) = 0b000 = DIV1
    // Verify: RCC_CFGR_PPRE1_DIV1 = 0x00000000 (all bits cleared)
    
    // PPRE2 (APB2 prescaler, bits 11-13): Set to DIV1 (0b000) so APB2 = HCLK
    // Same encoding as PPRE1
    RCC->CFGR &= ~(0x7 << 11);  // Clear PPRE2 bits (bits 11-13) = 0b000 = DIV1
    
    // Force write to ensure register update
    volatile uint32_t cfgr_check = RCC->CFGR;
    (void)cfgr_check;  // Suppress unused warning
    
    // Small delay to ensure prescaler settings are stable
    volatile int delay = 10;
    while (delay-- > 0) {
        __asm("nop");
    }
    
    // Configure and turn on PLL
    configurePLL();
    
    // Select PLL as clock source (bits 0-1: SW = 0b11 for PLL)
    RCC->CFGR |= (0b11 << 0);
    
    // Wait until PLL is selected as system clock (bits 2-3: SWS should be 0b11)
    while(!((RCC->CFGR >> 2) & 0b11));
    
    // Additional delay to ensure clock switch is complete
    delay = 100;
    while (delay-- > 0) {
        __asm("nop");
    }
    
    // CRITICAL: Re-apply prescaler settings AFTER clock switch to ensure they're active
    // Sometimes prescalers need to be set again after switching clock sources
    // Use read-modify-write to ensure proper setting
    uint32_t cfgr_temp = RCC->CFGR;
    cfgr_temp &= ~(0xF << 4);   // Clear HPRE bits = DIV1 (0b0000)
    cfgr_temp &= ~(0x7 << 8);   // Clear PPRE1 bits = DIV1 (0b000) - APB1 for DAC
    cfgr_temp &= ~(0x7 << 11);  // Clear PPRE2 bits = DIV1 (0b000)
    RCC->CFGR = cfgr_temp;      // Write entire register to ensure all prescalers are DIV1
    
    // Memory barrier to ensure register write completes
    __asm volatile("" ::: "memory");
    
    // Final delay to ensure prescalers are stable
    delay = 50;
    while (delay-- > 0) {
        __asm("nop");
    }
    
    // Now SYSCLK = 80MHz, HCLK = 80MHz, APB1 = 80MHz, APB2 = 80MHz
}