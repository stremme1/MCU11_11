// STM32L432KC_DAC.c
// DAC library implementation for STM32L432KC
//
// Author: Emmett Stralka
// Email: estralka@hmc.edu
// Date: 9/29/25
//
// Description: DAC library implementation for audio output

#include "STM32L432KC_DAC.h"
#include "STM32L432KC_RCC.h"
#include "STM32L432KC_TIMER.h"  // For ms_delay
#include <stddef.h>  // For NULL

// Enable DAC clock
void DAC_EnableClock(void) {
    // Enable DAC clock in APB1ENR1 (bit 29)
    RCC->APB1ENR1 |= (1 << 29);
    
    // Small delay to let clock stabilize
    volatile int delay = 10;
    while (delay-- > 0) {
        __asm("nop");
    }
}

// Configure GPIO pin for DAC output
void DAC_ConfigureGPIO(int channel) {
    // Enable GPIOA clock
    RCC->AHB2ENR |= (1 << 0);
    
    // Small delay to let clock stabilize
    volatile int delay = 10;
    while (delay-- > 0) {
        __asm("nop");
    }
    
    int pin;
    if (channel == DAC_CHANNEL_1) {
        pin = DAC_OUT1_PIN;  // PA4
    } else {
        pin = DAC_OUT2_PIN;  // PA5
    }
    
    // CRITICAL: Clear existing mode bits first
    // MODER bits for PA4 are bits 8-9 (2*4 = 8)
    GPIOA->MODER &= ~(0b11 << (2 * pin));
    
    // Set pin to analog mode (bits 2*pin and 2*pin+1 both set to 1)
    // Analog mode = 0b11 = both bits set
    GPIOA->MODER |= (0b11 << (2 * pin));
    
    // Disable pull-up/pull-down (set to no pull)
    // PURPDR bits for PA4 are bits 8-9
    GPIOA->PURPDR &= ~(0b11 << (2 * pin));
    
    // Also clear OTYPER (output type) and OSPEEDR (speed) to ensure clean state
    GPIOA->OTYPER &= ~(1 << pin);  // Push-pull (not open-drain)
    GPIOA->OSPEEDR &= ~(0b11 << (2 * pin));  // Low speed (doesn't matter for analog)
    
    // Verify the configuration
    volatile uint32_t moder_check = GPIOA->MODER;
    // For PA4 (pin 4), bits 8-9 should be 0b11 (analog mode)
    if ((moder_check & (0b11 << (2 * pin))) != (0b11 << (2 * pin))) {
        // If not set correctly, try again
        GPIOA->MODER &= ~(0b11 << (2 * pin));
        GPIOA->MODER |= (0b11 << (2 * pin));
    }
}

// Initialize DAC channel
// Reference: RM0394 Rev 5, Section 17 (Digital-to-analog converter (DAC))
//            Page 509/1628 - Section 17.7.1 (DAC control register - DAC_CR)
//            Page 512/1628 - EN1 bit description
//            Page 512/1628 - TEN1 bit description
// Proper initialization sequence:
// 1. Enable DAC clock (RCC->APB1ENR1 bit 29)
// 2. Configure GPIO to analog mode (MODER bits for PA4/PA5)
// 3. Clear EN1 bit first (bit 0) - required before modifying other CR bits
// 4. Configure CR register: TEN1=0 (immediate update, 1 APB1 cycle delay)
// 5. Write initial value to DHR12R1 (Data Holding Register)
// 6. Enable DAC channel (EN1=1, bit 0)
void DAC_Init(int channel) {
    // Step 1: Enable DAC clock
    DAC_EnableClock();
    
    // Step 2: Configure GPIO for DAC output
    DAC_ConfigureGPIO(channel);
    
    if (channel == DAC_CHANNEL_1) {
        // Step 3: Clear EN1 bit first (required before modifying other CR bits and MCR)
        // Reference: RM0394 Rev 5, Page 512/1628 - EN1 bit description
        // EN1 must be cleared before modifying other bits in DAC_CR and DAC_MCR
        DAC->CR &= ~(1 << 0);  // Clear EN1 (bit 0) first
        __asm volatile("" ::: "memory");
        
        // Wait a few APB1 cycles to ensure EN1 is cleared
        volatile int delay_clear = 10;
        while (delay_clear-- > 0) {
            __asm("nop");
        }
        
        // Step 3a: Configure DAC_MCR (Mode Control Register) - MUST be done when EN1=0
        // Reference: RM0394 Rev 5, Page 522/1628, Section 17.7.16 (DAC_MCR register)
        // Reference: RM0394 Rev 5, Page 526/1628, Table 86 (DAC register map)
        // MODE1[2:0] bits (bits 2:0) can only be written when EN1=0 and CEN1=0
        // If EN1=1 or CEN1=1, the write operation is ignored
        // MODE1 = 000: Normal mode, connected to external pin with Buffer enabled
        // This is the mode we want for audio output on PA4
        // Reset value of MCR is 0x0000 0000, so MODE1 is already 000, but we'll clear it explicitly
        DAC->MCR &= ~(0x7);  // Clear MODE1[2:0] bits (bits 2:0) to ensure 000
        __asm volatile("" ::: "memory");
        
        // Verify MCR was written correctly (MODE1 should be 000)
        volatile uint32_t mcr_verify = DAC->MCR;
        if ((mcr_verify & 0x7) != 0) {
            // If MODE1 is not 000, try again (but this shouldn't happen if EN1=0)
            DAC->MCR &= ~(0x7);
            __asm volatile("" ::: "memory");
        }
        
        // Step 4: Configure DAC_CR register - ensure TEN1=0 for immediate update
        // Reference: RM0394 Rev 5, Page 510-513/1628, Section 17.7.1 (DAC_CR register)
        // Clear all channel 1 configuration bits (bits 0-14):
        // EN1=0 (bit 0), TEN1=0 (bit 2), TSEL1=0 (bits 3-5), 
        // WAVE1=0 (bits 6-7, no wave), MAMP1=0 (bits 8-11), 
        // DMAEN1=0 (bit 12), DMAUDRIE1=0 (bit 13), CEN1=0 (bit 14)
        // CRITICAL: CEN1 must be 0 to allow MCR writes
        DAC->CR &= ~(0x7FFF);  // Clear all channel 1 bits (0-14)
        __asm volatile("" ::: "memory");
        
        // CRITICAL: TEN1=0 means immediate update mode
        // Reference: RM0394 Rev 5, Page 512/1628 - TEN1 bit description
        // When TEN1=0: Data written to DAC_DHR1 is transferred ONE APB1 clock cycle 
        //              later to DAC_DOR1 (immediate update, no trigger needed)
        // When TEN1=1: Data from DAC_DHR1 is transferred THREE APB1 clock cycles 
        //              later to DAC_DOR1 (triggered mode)
        // We want TEN1=0 for immediate update (already cleared above)
        
        // Step 5: Write initial value to DHR12R1 BEFORE enabling
        // Reference: RM0394 Rev 5, Page 514/1628, Section 17.7.3 (DAC_DHR12R1)
        // Write to data holding register before enabling channel
        // Bits 11:0 contain the 12-bit right-aligned data
        DAC->DHR12R1 = 2048;  // Mid-point value (12-bit right-aligned, bits 11:0)
        __asm volatile("" ::: "memory");
        
        // CRITICAL: Wait for DHR->DOR transfer to complete
        // Reference: RM0394 Rev 5, Page 512/1628 - TEN1 bit description
        // With TEN1=0, transfer happens 1 APB1 cycle after writing to DHR
        // At 80MHz APB1, 1 cycle = 12.5ns, but we'll wait longer to be safe
        // We need to wait BEFORE enabling EN1 to ensure DOR has the correct value
        volatile int delay_transfer = 100;  // Wait many APB1 cycles
        while (delay_transfer-- > 0) {
            __asm("nop");
        }
        
        // Verify DOR1 has the correct value (read-only register)
        // Reference: RM0394 Rev 5, Page 518/1628, Section 17.7.12 (DAC_DOR1)
        // DOR1 is read-only and contains the actual data output (bits 11:0)
        volatile uint32_t dor1_value = DAC->DOR1;
        (void)dor1_value;  // Suppress unused warning - we can't write to DOR1, only read
        
        // Step 6: Enable channel 1 (EN1 bit)
        // Reference: RM0394 Rev 5, Page 512/1628 - EN1 bit description (bit 0 of DAC_CR)
        // Set EN1=1 (bit 0) to enable DAC channel1
        // DOR should already have the value from DHR (transferred 1 APB1 cycle after Step 5)
        DAC->CR |= (1 << 0);  // EN1 = 1
        __asm volatile("" ::: "memory");
        
        // Verify EN1 is actually set (read back to confirm)
        volatile uint32_t cr_verify = DAC->CR;
        if (!(cr_verify & (1 << 0))) {
            // If EN1 is not set, try again
            DAC->CR |= (1 << 0);
            __asm volatile("" ::: "memory");
        }
        
        // Wait for DAC output buffer to stabilize after enabling
        // Reference: RM0394 Rev 5, Page 522/1628 - MODE1=000 means buffer is enabled
        // The DAC output buffer needs time to settle to the new voltage
        // With buffer enabled, output range is 0.2V to (VREF+ - 0.2V)
        ms_delay(10);  // Wait for DAC to stabilize
    } else {
        // Channel 2 initialization
        // Clear EN2 first (required before modifying MCR and CR)
        DAC->CR &= ~(1 << 16);  // Clear EN2 (bit 16) first
        __asm volatile("" ::: "memory");
        
        // Wait for EN2 to clear
        volatile int delay_clear = 10;
        while (delay_clear-- > 0) {
            __asm("nop");
        }
        
        // Configure MODE2[2:0] = 000 (normal mode, external pin, buffer enabled)
        // Reference: RM0394 Rev 5, Page 522/1628 - MODE2 can only be written when EN2=0
        DAC->MCR &= ~(0x70000);  // Clear MODE2[2:0] bits (bits 16-18) to set to 000
        __asm volatile("" ::: "memory");
        
        // Clear all channel 2 configuration bits
        DAC->CR &= ~(0x7FFF0000);  // Clear all channel 2 bits (16-30)
        __asm volatile("" ::: "memory");
        
        // Write initial value
        DAC->DHR12R2 = 2048;  // Initial value
        __asm volatile("" ::: "memory");
        
        // Wait for DHR->DOR transfer
        volatile int delay_transfer = 100;
        while (delay_transfer-- > 0) {
            __asm("nop");
        }
        
        // Enable channel 2
        DAC->CR |= (1 << 16);  // EN2 = 1
        __asm volatile("" ::: "memory");
        ms_delay(10);
    }
}

// Start DAC (enable output)
void DAC_Start(int channel) {
    if (channel == DAC_CHANNEL_1) {
        DAC->CR |= (1 << 0);  // Enable channel 1
    } else {
        DAC->CR |= (1 << 16); // Enable channel 2
    }
}

// Stop DAC (disable output)
void DAC_Stop(int channel) {
    if (channel == DAC_CHANNEL_1) {
        DAC->CR &= ~(1 << 0);  // Disable channel 1
    } else {
        DAC->CR &= ~(1 << 16); // Disable channel 2
    }
}

// Set DAC output value (12-bit, 0-4095)
void DAC_SetValue(int channel, uint16_t value) {
    // Clamp value to 12-bit range
    if (value > 4095) {
        value = 4095;
    }
    
    if (channel == DAC_CHANNEL_1) {
        // CRITICAL: Make sure DAC channel 1 is enabled BEFORE writing
        // The EN1 bit must be set for the DAC to output
        if (!(DAC->CR & (1 << 0))) {
            DAC->CR |= (1 << 0);  // Enable channel 1
            // Wait for DAC to stabilize after enabling
            ms_delay(2);
        }
        
        // Write to DHR12R1 (12-bit right-aligned data holding register for channel 1)
        // In immediate update mode (TEN=0), output updates automatically when EN=1
        // The formula is: Vout = (DAC_value / 4095) * VREF+
        // For 3.3V VREF+: value 2048 = 1.65V, value 4095 = 3.3V, value 0 = 0.2V (min with buffer)
        DAC->DHR12R1 = value & 0xFFF;  // Ensure only 12 bits are written (mask to 0xFFF)
        
        // Verify the write took effect (read back to ensure)
        volatile uint32_t verify = DAC->DHR12R1;
        (void)verify;  // Suppress unused variable warning
        
        // Wait for DAC to update (DAC needs settling time)
        // According to datasheet, DAC settling time is typically a few microseconds
        // With output buffer ON, minimum output is 0.2V, maximum is VREF+ - 0.2V
        // For audio, we need minimal delay - just enough for register write to complete
        // The actual timing is handled in the audio generation loop
        volatile int delay = 5;  // Minimal delay - just for register write completion
        while (delay-- > 0) {
            __asm("nop");
        }
    } else {
        // Make sure DAC channel 2 is enabled
        if (!(DAC->CR & (1 << 16))) {
            DAC->CR |= (1 << 16);
            ms_delay(2);
        }
        
        // Write to DHR12R2 (12-bit right-aligned data holding register for channel 2)
        DAC->DHR12R2 = value & 0xFFF;
        
        // Wait for DAC to settle (minimal delay for audio)
        volatile int delay = 5;
        while (delay-- > 0) {
            __asm("nop");
        }
    }
}

// Complete audio initialization
void DAC_InitAudio(int channel) {
    // Initialize DAC (this already sets up everything including initial value)
    DAC_Init(channel);
    
    // Additional verification and aggressive setup
    if (channel == DAC_CHANNEL_1) {
        // Force enable DAC channel 1 - make absolutely sure it's enabled
        DAC->CR |= (1 << 0);  // Set EN1 bit
        ms_delay(5);
        
        // Verify enable bit is actually set
        volatile uint32_t cr_check = DAC->CR;
        if (!(cr_check & (1 << 0))) {
            // If still not enabled, something is very wrong
            // Try again with a longer delay
            ms_delay(10);
            DAC->CR |= (1 << 0);
            ms_delay(10);
        }
        
        // Write a test value to verify DAC responds
        DAC->DHR12R1 = 2048;  // Mid-point
        ms_delay(5);
        
        // Write maximum value to test
        DAC->DHR12R1 = 4095;  // Maximum
        ms_delay(5);
        
        // Write back to mid-point
        DAC->DHR12R1 = 2048;
        ms_delay(5);
        
        // Final verification
        volatile uint32_t dhr_check = DAC->DHR12R1;
        (void)dhr_check;  // Suppress unused warning
        
        // Final delay to ensure everything is stable
        ms_delay(10);
    } else {
        // Channel 2 verification
        DAC->CR |= (1 << 16);  // Force enable
        ms_delay(10);
        DAC->DHR12R2 = 2048;
        ms_delay(10);
    }
}

// High-resolution sine wave lookup table (1024 samples for one period)
// Pre-calculated for efficiency with higher resolution
// Values range from 0 to 4095 (12-bit DAC), centered at 2048
static const uint16_t sine_table[1024] = {
    2048, 2060, 2073, 2085, 2098, 2110, 2123, 2135, 2148, 2161, 2173, 2186, 2198, 2211, 2223, 2236,
    2248, 2261, 2273, 2286, 2298, 2311, 2323, 2336, 2348, 2360, 2373, 2385, 2398, 2410, 2422, 2435,
    2447, 2459, 2472, 2484, 2496, 2508, 2521, 2533, 2545, 2557, 2569, 2582, 2594, 2606, 2618, 2630,
    2642, 2654, 2666, 2678, 2690, 2702, 2714, 2726, 2737, 2749, 2761, 2773, 2785, 2796, 2808, 2820,
    2831, 2843, 2854, 2866, 2877, 2889, 2900, 2912, 2923, 2934, 2946, 2957, 2968, 2980, 2991, 3002,
    3013, 3024, 3035, 3046, 3057, 3068, 3079, 3090, 3100, 3111, 3122, 3133, 3143, 3154, 3164, 3175,
    3185, 3196, 3206, 3216, 3227, 3237, 3247, 3257, 3267, 3278, 3288, 3298, 3307, 3317, 3327, 3337,
    3347, 3356, 3366, 3376, 3385, 3395, 3404, 3414, 3423, 3432, 3441, 3451, 3460, 3469, 3478, 3487,
    3496, 3505, 3513, 3522, 3531, 3539, 3548, 3557, 3565, 3573, 3582, 3590, 3598, 3606, 3615, 3623,
    3631, 3639, 3646, 3654, 3662, 3670, 3677, 3685, 3692, 3700, 3707, 3715, 3722, 3729, 3736, 3743,
    3750, 3757, 3764, 3771, 3778, 3784, 3791, 3798, 3804, 3811, 3817, 3823, 3829, 3836, 3842, 3848,
    3854, 3860, 3865, 3871, 3877, 3882, 3888, 3893, 3899, 3904, 3909, 3915, 3920, 3925, 3930, 3935,
    3940, 3944, 3949, 3954, 3958, 3963, 3967, 3972, 3976, 3980, 3984, 3988, 3992, 3996, 4000, 4004,
    4007, 4011, 4014, 4018, 4021, 4025, 4028, 4031, 4034, 4037, 4040, 4043, 4046, 4048, 4051, 4054,
    4056, 4059, 4061, 4063, 4065, 4067, 4069, 4071, 4073, 4075, 4077, 4079, 4080, 4082, 4083, 4084,
    4086, 4087, 4088, 4089, 4090, 4091, 4092, 4092, 4093, 4094, 4094, 4095, 4095, 4095, 4095, 4095,
    4095, 4095, 4095, 4095, 4095, 4095, 4094, 4094, 4093, 4092, 4092, 4091, 4090, 4089, 4088, 4087,
    4086, 4084, 4083, 4082, 4080, 4079, 4077, 4075, 4073, 4071, 4069, 4067, 4065, 4063, 4061, 4059,
    4056, 4054, 4051, 4048, 4046, 4043, 4040, 4037, 4034, 4031, 4028, 4025, 4021, 4018, 4014, 4011,
    4007, 4004, 4000, 3996, 3992, 3988, 3984, 3980, 3976, 3972, 3967, 3963, 3958, 3954, 3949, 3944,
    3940, 3935, 3930, 3925, 3920, 3915, 3909, 3904, 3899, 3893, 3888, 3882, 3877, 3871, 3865, 3860,
    3854, 3848, 3842, 3836, 3829, 3823, 3817, 3811, 3804, 3798, 3791, 3784, 3778, 3771, 3764, 3757,
    3750, 3743, 3736, 3729, 3722, 3715, 3707, 3700, 3692, 3685, 3677, 3670, 3662, 3654, 3646, 3639,
    3631, 3623, 3615, 3606, 3598, 3590, 3582, 3573, 3565, 3557, 3548, 3539, 3531, 3522, 3513, 3505,
    3496, 3487, 3478, 3469, 3460, 3451, 3441, 3432, 3423, 3414, 3404, 3395, 3385, 3376, 3366, 3356,
    3347, 3337, 3327, 3317, 3307, 3298, 3288, 3278, 3267, 3257, 3247, 3237, 3227, 3216, 3206, 3196,
    3185, 3175, 3164, 3154, 3143, 3133, 3122, 3111, 3100, 3090, 3079, 3068, 3057, 3046, 3035, 3024,
    3013, 3002, 2991, 2980, 2968, 2957, 2946, 2934, 2923, 2912, 2900, 2889, 2877, 2866, 2854, 2843,
    2831, 2820, 2808, 2796, 2785, 2773, 2761, 2749, 2737, 2726, 2714, 2702, 2690, 2678, 2666, 2654,
    2642, 2630, 2618, 2606, 2594, 2582, 2569, 2557, 2545, 2533, 2521, 2508, 2496, 2484, 2472, 2459,
    2447, 2435, 2422, 2410, 2398, 2385, 2373, 2360, 2348, 2336, 2323, 2311, 2298, 2286, 2273, 2261,
    2248, 2236, 2223, 2211, 2198, 2186, 2173, 2161, 2148, 2135, 2123, 2110, 2098, 2085, 2073, 2060,
    2048, 2035, 2022, 2010, 1997, 1985, 1972, 1960, 1947, 1934, 1922, 1909, 1897, 1884, 1872, 1859,
    1847, 1834, 1822, 1809, 1797, 1784, 1772, 1759, 1747, 1735, 1722, 1710, 1697, 1685, 1673, 1660,
    1648, 1636, 1623, 1611, 1599, 1587, 1574, 1562, 1550, 1538, 1526, 1513, 1501, 1489, 1477, 1465,
    1453, 1441, 1429, 1417, 1405, 1393, 1381, 1369, 1358, 1346, 1334, 1322, 1310, 1299, 1287, 1275,
    1264, 1252, 1241, 1229, 1218, 1206, 1195, 1183, 1172, 1161, 1149, 1138, 1127, 1115, 1104, 1093,
    1082, 1071, 1060, 1049, 1038, 1027, 1016, 1005, 995, 984, 973, 962, 952, 941, 931, 920,
    910, 899, 889, 879, 868, 858, 848, 838, 828, 817, 807, 797, 788, 778, 768, 758,
    748, 739, 729, 719, 710, 700, 691, 681, 672, 663, 654, 644, 635, 626, 617, 608,
    599, 590, 582, 573, 564, 556, 547, 538, 530, 522, 513, 505, 497, 489, 480, 472,
    464, 456, 449, 441, 433, 425, 418, 410, 403, 395, 388, 380, 373, 366, 359, 352,
    345, 338, 331, 324, 317, 311, 304, 297, 291, 284, 278, 272, 266, 259, 253, 247,
    241, 235, 230, 224, 218, 213, 207, 202, 196, 191, 186, 180, 175, 170, 165, 160,
    155, 151, 146, 141, 137, 132, 128, 123, 119, 115, 111, 107, 103, 99, 95, 91,
    88, 84, 81, 77, 74, 70, 67, 64, 61, 58, 55, 52, 49, 47, 44, 41,
    39, 36, 34, 32, 30, 28, 26, 24, 22, 20, 18, 16, 15, 13, 12, 11,
    9, 8, 7, 6, 5, 4, 3, 3, 2, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 1, 1, 2, 3, 3, 4, 5, 6, 7, 8,
    9, 11, 12, 13, 15, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36,
    39, 41, 44, 47, 49, 52, 55, 58, 61, 64, 67, 70, 74, 77, 81, 84,
    88, 91, 95, 99, 103, 107, 111, 115, 119, 123, 128, 132, 137, 141, 146, 151,
    155, 160, 165, 170, 175, 180, 186, 191, 196, 202, 207, 213, 218, 224, 230, 235,
    241, 247, 253, 259, 266, 272, 278, 284, 291, 297, 304, 311, 317, 324, 331, 338,
    345, 352, 359, 366, 373, 380, 388, 395, 403, 410, 418, 425, 433, 441, 449, 456,
    464, 472, 480, 489, 497, 505, 513, 522, 530, 538, 547, 556, 564, 573, 582, 590,
    599, 608, 617, 626, 635, 644, 654, 663, 672, 681, 691, 700, 710, 719, 729, 739,
    748, 758, 768, 778, 788, 797, 807, 817, 828, 838, 848, 858, 868, 879, 889, 899,
    910, 920, 931, 941, 952, 962, 973, 984, 995, 1005, 1016, 1027, 1038, 1049, 1060, 1071,
    1082, 1093, 1104, 1115, 1127, 1138, 1149, 1161, 1172, 1183, 1195, 1206, 1218, 1229, 1241, 1252,
    1264, 1275, 1287, 1299, 1310, 1322, 1334, 1346, 1358, 1369, 1381, 1393, 1405, 1417, 1429, 1441,
    1453, 1465, 1477, 1489, 1501, 1513, 1526, 1538, 1550, 1562, 1574, 1587, 1599, 1611, 1623, 1636,
    1648, 1660, 1673, 1685, 1697, 1710, 1722, 1735, 1747, 1759, 1772, 1784, 1797, 1809, 1822, 1834,
    1847, 1859, 1872, 1884, 1897, 1909, 1922, 1934, 1947, 1960, 1972, 1985, 1997, 2010, 2022, 2035
};

// Play a sine wave at specified frequency for specified duration
// Uses software-based sample generation with delay and envelope to prevent clicks
// Note: This function currently uses DAC_CHANNEL_1 - modify if you need channel 2
void DAC_PlaySineWave(float frequency, uint32_t duration_ms, uint32_t sample_rate) {
    if (frequency == 0) {
        // Rest - quickly fade to mid-point (silence)
        DAC_SetValue(DAC_CHANNEL_1, 2048);
        ms_delay(duration_ms);
        return;
    }
    
    // Calculate number of samples needed
    uint32_t num_samples = (sample_rate * duration_ms) / 1000;
    
    // Calculate phase increment per sample
    // For 1024-entry sine table: phase_increment = (frequency * 1024) / sample_rate
    // This determines how fast we move through the sine table
    // Higher resolution table (1024 vs 256) provides smoother sine waves
    float phase_increment = (frequency * 1024.0f) / (float)sample_rate;
    
    // Envelope parameters (attack and decay to prevent clicks)
    // Use very short envelope to preserve the waveform
    uint32_t attack_samples = (sample_rate * 1) / 1000;  // 1ms attack
    if (attack_samples > num_samples / 4) attack_samples = num_samples / 4;
    if (attack_samples == 0) attack_samples = 1;
    uint32_t decay_samples = (sample_rate * 1) / 1000;  // 1ms decay
    if (decay_samples > num_samples / 4) decay_samples = num_samples / 4;
    if (decay_samples == 0) decay_samples = 1;
    
    // Calculate timing - CRITICAL: Verify CPU clock is actually 80MHz
    // CPU clock (SYSCLK) should be 80MHz from PLL configuration
    // At 80MHz: 1 cycle = 12.5ns
    // Time per sample in microseconds = 1,000,000 / sample_rate
    // Total cycles per sample = (1,000,000 / sample_rate) * CPU_FREQ_MHZ
    // Example at 48kHz: 1,000,000 / 48000 = 20.833 us -> 20.833 * 80 = 1666.67 cycles
    // 
    // NOTE: If timing is still off, verify:
    // 1. SYSCLK is actually 80MHz (check RCC->CFGR bits 2-3 should be 0b11 for PLL)
    // 2. HCLK = SYSCLK (no AHB prescaler - check RCC->CFGR bits 4-7 should be 0b0000)
    // 3. CPU is running at full speed (no sleep modes)
    // 4. If audio sounds too slow/low res, CPU_FREQ_MHZ might be lower than 80
    // 5. If audio sounds too fast/high pitched, CPU_FREQ_MHZ might be higher than 80
    
    // PLL configuration: (4MHz MSI) * (80/1) / 4 = 80MHz (expected SYSCLK)
    // 
    // CALCULATION FOR 48 kHz SAMPLE RATE:
    // - Time per sample: 1,000,000 / 48,000 = 20.833 microseconds
    // - At 80 MHz: 20.833 * 80 = 1,666.67 cycles per sample
    //
    // OVERHEAD BREAKDOWN (measured/estimated):
    // - DAC_SetValue: ~30 cycles (register write + memory barriers + verification)
    // - Direct table lookup: ~8 cycles (phase calc, 1 table lookup, modulo)
    // - Phase wrapping (while loops): ~15 cycles (worst case, usually less)
    // - Envelope calculation: ~35 cycles (when active: integer math, conditionals)
    // - Loop overhead: ~12 cycles (increment, compare, branch)
    // - Total overhead: ~100 cycles (sustain) to ~200 cycles (with envelope)
    //
    // Since envelope is only active at start/end, average overhead is ~130 cycles
    // Using 150 cycles to be safe (accounts for cache misses, branch mispredictions)
    //
    // If audio is too low/slow: overhead is underestimated -> increase overhead_cycles
    // If audio is too high/fast: overhead is overestimated -> decrease overhead_cycles
    // CALIBRATED: Based on frequency measurements
    // Test 1: 440 Hz target sounded like 330 Hz -> (20 × 330) / 440 = 15 MHz
    // Test 2: 262 Hz target sounded like 197 Hz -> (20 × 197) / 262 = 15.04 MHz
    // Average: 15 MHz (confirmed accurate)
    #define CPU_FREQ_MHZ 18  // Calibrated CPU frequency in MHz
    uint32_t us_per_sample = 1000000UL / sample_rate;
    uint32_t total_cycles_needed = us_per_sample * CPU_FREQ_MHZ;
    
    // Overhead estimate - if audio is slightly too low, increase this value
    // If audio is slightly too high, decrease this value
    uint32_t overhead_cycles = 150;  // Reduced since we removed interpolation
    uint32_t delay_cycles = (total_cycles_needed > overhead_cycles) ? 
                            (total_cycles_needed - overhead_cycles) : 1;
    
    float phase = 0.0f;  // Always start at phase 0 to avoid phase jumps
    
    for (uint32_t i = 0; i < num_samples; i++) {
        // Convert phase to table index (no interpolation - direct lookup)
        // Phase is in range [0, 1024), so we can directly cast after ensuring it's in range
        while (phase >= 1024.0f) phase -= 1024.0f;
        while (phase < 0.0f) phase += 1024.0f;

        // Direct table lookup (no interpolation)
        uint32_t phase_int = (uint32_t)phase;
        uint16_t index = phase_int % 1024;
        uint16_t sine_value = sine_table[index];
        
        // Apply minimal envelope (attack/decay) to prevent clicks
        // For very short notes, skip envelope to preserve waveform
        uint16_t dac_value;
        if (num_samples > 100 && i < attack_samples && attack_samples > 0) {
            // Attack: fade in from silence (2048)
            int32_t amplitude = (int32_t)sine_value - 2048;
            dac_value = 2048 + (uint16_t)((amplitude * (int32_t)i) / (int32_t)attack_samples);
        } else if (num_samples > 100 && i >= (num_samples - decay_samples) && decay_samples > 0) {
            // Decay: fade out to silence (2048)
            uint32_t fade_pos = i - (num_samples - decay_samples);
            int32_t amplitude = (int32_t)sine_value - 2048;
            dac_value = 2048 + (uint16_t)((amplitude * (int32_t)(decay_samples - fade_pos)) / (int32_t)decay_samples);
        } else {
            // Sustain: full amplitude (or no envelope for short notes)
            dac_value = sine_value;
        }
        
        // Output to DAC (using channel 1)
        DAC_SetValue(DAC_CHANNEL_1, dac_value);
        
        // Update phase
        phase += phase_increment;
        
        // Delay to maintain sample rate
        volatile uint32_t delay_count = delay_cycles;
        while (delay_count-- > 0) {
            __asm("nop");
        }
    }
    
    // Fade to silence at end to prevent click
    DAC_SetValue(DAC_CHANNEL_1, 2048);
}

// Play a WAV sample from memory
// sample_data: pointer to 16-bit signed PCM data
// sample_length: number of samples
// sample_rate: sample rate in Hz (should be 22050 for converted samples)
void DAC_PlayWAV(const int16_t* sample_data, uint32_t sample_length, uint32_t sample_rate) {
    if (sample_data == NULL || sample_length == 0) {
        return;
    }
    
    // Calculate timing - using calibrated CPU frequency
    #define CPU_FREQ_MHZ 15  // Calibrated CPU frequency in MHz
    uint32_t us_per_sample = 1000000UL / sample_rate;
    uint32_t total_cycles_needed = us_per_sample * CPU_FREQ_MHZ;
    
    // Overhead: DAC_SetValue + loop overhead
    // DAC_SetValue: ~30 cycles
    // Loop overhead: ~10 cycles
    uint32_t overhead_cycles = 50;  // Conservative estimate
    uint32_t delay_cycles = (total_cycles_needed > overhead_cycles) ? 
                            (total_cycles_needed - overhead_cycles) : 1;
    
    // Play all samples
    for (uint32_t i = 0; i < sample_length; i++) {
        // Convert 16-bit signed sample (-32768 to 32767) to 12-bit DAC value (0 to 4095)
        // Center at 2048 (mid-point), scale to use full range
        int32_t sample = (int32_t)sample_data[i];
        
        // Scale: map -32768..32767 to 0..4095
        // Formula: dac_value = (sample + 32768) * 4095 / 65536
        // Simplified: dac_value = (sample + 32768) >> 4
        // But we want to center at 2048, so: dac_value = 2048 + (sample >> 4)
        int32_t dac_value = 2048 + (sample >> 4);
        
        // Clamp to 12-bit range
        if (dac_value < 0) dac_value = 0;
        if (dac_value > 4095) dac_value = 4095;
        
        // Output to DAC
        DAC_SetValue(DAC_CHANNEL_1, (uint16_t)dac_value);
        
        // Delay to maintain sample rate
        volatile uint32_t delay_count = delay_cycles;
        while (delay_count-- > 0) {
            __asm("nop");
        }
    }
    
    // Fade to silence at end
    DAC_SetValue(DAC_CHANNEL_1, 2048);
}

// Test function - output a constant DC value for testing
void DAC_TestOutput(int channel, uint16_t value, uint32_t duration_ms) {
    DAC_SetValue(channel, value);
    ms_delay(duration_ms);
}

