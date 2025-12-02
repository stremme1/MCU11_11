// STM32L432KC_TIMER.h
// Timer library for STM32L432KC
//
// Author: Emmett Stralka
// Email: estralka@hmc.edu
// Date: 9/29/25
//
// Description: Timer library for PWM generation and timing functions

#ifndef STM32L4_TIMER_H
#define STM32L4_TIMER_H

#include <stdint.h>

// Timer definitions
#define __IO volatile

// Base addresses
#define TIM2_BASE (0x40000000UL)
#define GPIOA_BASE (0x48000000UL)

// Timer register structure
typedef struct {
    __IO uint32_t CR1;      // Control register 1
    __IO uint32_t CR2;      // Control register 2
    __IO uint32_t SMCR;     // Slave mode control register
    __IO uint32_t DIER;     // DMA/interrupt enable register
    __IO uint32_t SR;       // Status register
    __IO uint32_t EGR;      // Event generation register
    __IO uint32_t CCMR1;    // Capture/compare mode register 1
    __IO uint32_t CCMR2;    // Capture/compare mode register 2
    __IO uint32_t CCER;     // Capture/compare enable register
    __IO uint32_t CNT;      // Counter
    __IO uint32_t PSC;      // Prescaler
    __IO uint32_t ARR;      // Auto-reload register
    uint32_t      RESERVED1; // Reserved
    __IO uint32_t CCR1;     // Capture/compare register 1
    __IO uint32_t CCR2;     // Capture/compare register 2
    __IO uint32_t CCR3;     // Capture/compare register 3
    __IO uint32_t CCR4;     // Capture/compare register 4
    uint32_t      RESERVED2; // Reserved
    __IO uint32_t DCR;      // DMA control register
    __IO uint32_t DMAR;     // DMA address for full transfer
} TIM_TypeDef;

#define TIM2 ((TIM_TypeDef *) TIM2_BASE)

// GPIO register structure for direct access
typedef struct {
    __IO uint32_t MODER;   // GPIO Offset 0x00 GPIO port mode register
    __IO uint32_t OTYPER;  // GPIO Offset 0x04
    __IO uint32_t OSPEEDR; // GPIO Offset 0x08
    __IO uint32_t PURPDR;  // GPIO Offset 0x0C
    __IO uint32_t IDR;     // GPIO Offset 0x10
    __IO uint32_t ODR;     // GPIO Offset 0x14
    __IO uint32_t BSRR;    // GPIO Offset 0x18
    __IO uint32_t LCKR;    // GPIO Offset 0x1C
    __IO uint32_t AFRL;    // GPIO Offset 0x20
    __IO uint32_t AFRH;    // GPIO Offset 0x24
} GPIO_TypeDef;

#define GPIOA ((GPIO_TypeDef *) GPIOA_BASE)

// Function prototypes
void TIM2_Init(void);
void TIM2_Start(void);
void TIM2_Stop(void);
void TIM2_SetFrequency(uint32_t frequency);
void ms_delay(int ms);
void TIM2_EnableGPIOClock(void);
void TIM2_ConfigurePA5(void);  // Configure PA5 for TIM2_CH1
void TIM2_InitAudio(void);  // Complete audio setup function
void TIM2_Silence(void);  // Set PWM duty cycle to 0% for silence

#endif
