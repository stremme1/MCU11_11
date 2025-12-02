// BNO085_SPI_HAL.h
// SPI HAL implementation for BNO085 sensor on STM32L432KC
//
// Implements sh2_Hal_t interface for SHTP protocol communication

#ifndef BNO085_SPI_HAL_H
#define BNO085_SPI_HAL_H

#include "sh2_hal.h"
#include <stdint.h>

// Pin definitions for BNO085
#define BNO085_RST_PIN   0   // PA0 - NRST (Reset pin, active low)
#define BNO085_INT_PIN   1   // PA1 - H_INTN (Interrupt pin, active low, data ready)
#define BNO085_CS_PIN    11  // PA11 - Chip Select (active low)
#define BNO085_WAKE_PIN  12  // PA12 - PS0/WAKE (Wake signal, active low, must stay HIGH during init)

// SPI1 pin definitions
#define SPI1_SCK_PIN     3   // PB3 - SPI1_SCK
#define SPI1_MOSI_PIN    5   // PB5 - SPI1_MOSI
#define SPI1_MISO_PIN    4   // PB4 - SPI1_MISO

// Function prototypes
int BNO085_SPI_HAL_Init(sh2_Hal_t *hal);
void BNO085_SPI_HAL_DeInit(void);
void BNO085_HardwareReset(void);  // Public function for hardware reset

#endif // BNO085_SPI_HAL_H

