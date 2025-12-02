# Lab4 - Für Elise Player for STM32L432KC

This project implements a Für Elise player using the STM32L432KC microcontroller with PWM-based square wave generation and hardware volume control.

## BPM Calculation
- Simplified approach: Fixed 1kHz square wave output
- Original durations used: 125ms, 250ms, 375ms, 500ms
- No complex frequency calculations needed

## Technical Constraints (Player Implementation)
- **Min duration**: 1ms (ms_delay() parameter type: int)
- **Max duration**: 65,535ms (16-bit integer limit: 2^16 - 1 = 65,535)
- **Min frequency**: 1Hz (timer minimum: 1MHz ÷ 1,000,000 = 1Hz)
- **Max frequency**: 1MHz (timer frequency: 80MHz ÷ 80 = 1MHz)
- **Actual output**: Fixed 1kHz square wave (ARR = 1000, 1MHz ÷ 1000 = 1kHz)
- **Timer resolution**: 1μs (1MHz timer frequency: 1/1,000,000 = 1μs)
- **Duty cycle**: 50% (CCR1 = ARR/2 = 1000/2 = 500)

The system uses 80MHz system clock with 1MHz timer frequency for precise audio generation. Hardware volume control via potentiometer eliminates need for ADC processing.