// STM32L432KC_DAC.h
// DAC library for STM32L432KC
//
// Author: Emmett Stralka
// Email: estralka@hmc.edu
// Date: 9/29/25
//
// Description: DAC library for audio output generation

#ifndef STM32L4_DAC_H
#define STM32L4_DAC_H

#include <stdint.h>
#include "STM32L432KC_TIMER.h"  // For GPIO_TypeDef, GPIOA, and __IO definitions

// Base addresses
#define APB1PERIPH_BASE (0x40000000UL)
#define DAC_BASE (APB1PERIPH_BASE + 0x7400UL)

// DAC register structure
typedef struct {
    __IO uint32_t CR;          // DAC control register, Address offset: 0x00
    __IO uint32_t SWTRIGR;     // DAC software trigger register, Address offset: 0x04
    __IO uint32_t DHR12R1;     // DAC channel1 12-bit right-aligned data holding register, Address offset: 0x08
    __IO uint32_t DHR12L1;     // DAC channel1 12-bit left aligned data holding register, Address offset: 0x0C
    __IO uint32_t DHR8R1;      // DAC channel1 8-bit right aligned data holding register, Address offset: 0x10
    __IO uint32_t DHR12R2;     // DAC channel2 12-bit right-aligned data holding register, Address offset: 0x14
    __IO uint32_t DHR12L2;     // DAC channel2 12-bit left aligned data holding register, Address offset: 0x18
    __IO uint32_t DHR8R2;      // DAC channel2 8-bit right-aligned data holding register, Address offset: 0x1C
    __IO uint32_t DHR12RD;     // Dual DAC 12-bit right-aligned data holding register, Address offset: 0x20
    __IO uint32_t DHR12LD;     // Dual DAC 12-bit left aligned data holding register, Address offset: 0x24
    __IO uint32_t DHR8RD;      // Dual DAC 8-bit right aligned data holding register, Address offset: 0x28
    __IO uint32_t DOR1;        // DAC channel1 data output register, Address offset: 0x2C
    __IO uint32_t DOR2;        // DAC channel2 data output register, Address offset: 0x30
    __IO uint32_t SR;          // DAC status register, Address offset: 0x34
    __IO uint32_t CCR;         // DAC calibration control register, Address offset: 0x38
    __IO uint32_t MCR;         // DAC mode control register, Address offset: 0x3C
    __IO uint32_t SHSR1;       // DAC Sample and Hold sample time register 1, Address offset: 0x40
    __IO uint32_t SHSR2;       // DAC Sample and Hold sample time register 2, Address offset: 0x44
    __IO uint32_t SHHR;        // DAC Sample and Hold hold time register, Address offset: 0x48
    __IO uint32_t SHRR;        // DAC Sample and Hold refresh time register, Address offset: 0x4C
} DAC_TypeDef;

#define DAC ((DAC_TypeDef *) DAC_BASE)

// GPIO_TypeDef and GPIOA are defined in STM32L432KC_TIMER.h

// DAC channel definitions
#define DAC_CHANNEL_1 1
#define DAC_CHANNEL_2 2

// DAC output pins
#define DAC_OUT1_PIN 4  // PA4
#define DAC_OUT2_PIN 5  // PA5

// Function prototypes
void DAC_EnableClock(void);
void DAC_ConfigureGPIO(int channel);
void DAC_Init(int channel);
void DAC_Start(int channel);
void DAC_Stop(int channel);
void DAC_SetValue(int channel, uint16_t value);
void DAC_InitAudio(int channel);
void DAC_PlaySineWave(float frequency, uint32_t duration_ms, uint32_t sample_rate);
void DAC_PlayWAV(const int16_t* sample_data, uint32_t sample_length, uint32_t sample_rate);
void DAC_TestOutput(int channel, uint16_t value, uint32_t duration_ms);  // Test function - output constant DC value

#endif

