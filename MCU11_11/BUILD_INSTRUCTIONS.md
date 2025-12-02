# Build Instructions - Adding UART Debug Module

## Issue
The linker is reporting undefined symbols for UART functions because `STM32L432KC_UART.c` is not being compiled.

## Solution
You need to add `STM32L432KC_UART.c` to your project build configuration.

### For Embedded Studio / SEGGER:
1. Right-click on your project in the Project Explorer
2. Select "Add Existing File..." or "Add File..."
3. Navigate to the `MCU11_11` folder
4. Select `STM32L432KC_UART.c`
5. Click "Add"
6. Rebuild the project

### Alternative: Check Project Settings
1. Open Project Settings / Build Configuration
2. Go to "Source Files" or "Compile" section
3. Ensure `STM32L432KC_UART.c` is listed and checked
4. If not present, add it manually

## Files That Should Be Compiled:
- main.c
- BNO085_SPI_HAL.c
- drum_detection.c
- STM32L432KC_DAC.c
- STM32L432KC_FLASH.c
- STM32L432KC_GPIO.c
- STM32L432KC_RCC.c
- STM32L432KC_TIMER.c
- **STM32L432KC_UART.c** ← This one is missing!
- sh2.c
- sh2_SensorValue.c
- sh2_util.c
- shtp.c
- All wav_arrays/*.c files

## Verification
After adding the file, rebuild the project. The linker errors should disappear.

