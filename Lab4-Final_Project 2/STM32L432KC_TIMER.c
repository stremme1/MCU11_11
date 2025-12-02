// STM32L432KC_TIMER.c
// Timer library implementation for STM32L432KC
//
// Author: Emmett Stralka
// Email: estralka@hmc.edu
// Date: 9/29/25
//
// Description: Timer library implementation for PWM generation

#include "STM32L432KC_TIMER.h"
#include "STM32L432KC_RCC.h"

// Initialize TIM2 for PWM generation
void TIM2_Init(void) {
    // Enable TIM2 clock
    RCC->APB1ENR1 |= (1 << 0);
    
    // Configure for 1MHz timer frequency (80MHz / 80)
    TIM2->PSC = 79;  // 80MHz / 80 = 1MHz
    
    // Set default values
    TIM2->ARR = 1000;  // Default 1kHz
    TIM2->CCR1 = 500;  // 50% duty cycle
    
    // Configure PWM mode 1
    TIM2->CCMR1 |= (0b110 << 4) | (1 << 3);  // PWM mode 1, preload enable
    TIM2->CCER |= (1 << 0);  // Enable channel 1
    TIM2->CR1 |= (1 << 7);   // Auto-reload preload enable
}

// Start timer
void TIM2_Start(void) {
    TIM2->CR1 |= (1 << 0);
}

// Stop timer
void TIM2_Stop(void) {
    TIM2->CR1 &= ~(1 << 0);
}

// Set frequency for PWM output
void TIM2_SetFrequency(uint32_t frequency) {
    if (frequency == 0) {
        TIM2_Silence();
        return;
    }
    
    // Re-enable the PWM output channel
    TIM2->CCER |= (1 << 0);  // Enable channel 1 output
    
    // Calculate timer period for desired frequency
    // Timer frequency = 1MHz, so period = 1,000,000 / frequency
    uint32_t period = 1000000 / frequency;
    
    // Update timer period and duty cycle
    TIM2->ARR = period;
    TIM2->CCR1 = period / 2;  // 50% duty cycle
    
    // Start timer
    TIM2_Start();
}

// Function for dummy delay by executing nops
// Calibrated for 80MHz system clock
void ms_delay(int ms) {
   while (ms-- > 0) {
      volatile int x=8000;  // 8,000 NOPs for 1ms at 80MHz
      while (x-- > 0)
         __asm("nop");
   }
}

// Enable GPIOA clock for TIM2_CH1 (PA5)
void TIM2_EnableGPIOClock(void) {
    RCC->AHB2ENR |= (1 << 0);  // Enable GPIOA clock
}

// Configure PA5 as alternate function for TIM2_CH1
void TIM2_ConfigurePA5(void) {
    // Set PA5 to alternate function mode
    GPIOA->MODER &= ~(0b11 << 10);  // Clear bits 11:10 (PA5)
    GPIOA->MODER |= (0b10 << 10);   // Set bits 11:10 to 10 (alternate function)
    
    // Set alternate function to AF1 (TIM2_CH1)
    GPIOA->AFRL &= ~(0b1111 << 20);  // Clear bits 23:20 (PA5)
    GPIOA->AFRL |= (0b0001 << 20);   // Set bits 23:20 to 0001 (AF1 = TIM2_CH1)
}


// Disable PWM output channel for complete silence
void TIM2_Silence(void) {
    // Stop timer first to avoid glitches
    TIM2_Stop();
    // Disable the PWM output channel
    TIM2->CCER &= ~(1 << 0);  // Disable channel 1 output
}

// Complete audio initialization function
void TIM2_InitAudio(void) {
    // Enable GPIOA clock
    TIM2_EnableGPIOClock();
    
    // Configure PA5 as alternate function for TIM2_CH1
    TIM2_ConfigurePA5();
    
    // Initialize TIM2
    TIM2_Init();
}
