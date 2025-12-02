// BNO085_SPI_HAL.c
// SPI HAL implementation for BNO085 sensor on STM32L432KC
//
// Implements sh2_Hal_t interface for SHTP protocol communication

#include "BNO085_SPI_HAL.h"
#include "STM32L432KC_RCC.h"
#include "STM32L432KC_TIMER.h"  // Provides GPIOA, GPIO_TypeDef, and ms_delay
#include "STM32L432KC_RTT.h"    // For debug output
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  // For NULL definition

// SPI1 base address
#define SPI1_BASE (0x40013000UL)

// SPI register structure
typedef struct {
    volatile uint32_t CR1;      // Control register 1
    volatile uint32_t CR2;      // Control register 2
    volatile uint32_t SR;       // Status register
    volatile uint32_t DR;       // Data register
    volatile uint32_t CRCPR;    // CRC polynomial register
    volatile uint32_t RXCRCR;   // RX CRC register
    volatile uint32_t TXCRCR;   // TX CRC register
} SPI_TypeDef;

#define SPI1 ((SPI_TypeDef *) SPI1_BASE)

// SPI status register bits
#define SPI_SR_TXE   (1 << 1)  // Transmit buffer empty
#define SPI_SR_RXNE  (1 << 0)  // Receive buffer not empty

// GPIOB base address (GPIOA already defined in TIMER.h)
#define GPIOB_BASE (0x48000400UL)
#define GPIOB ((GPIO_TypeDef *) GPIOB_BASE)

// SPI status register bits
#define SPI_SR_RXNE  (1 << 0)  // Receive buffer not empty
#define SPI_SR_TXE   (1 << 1)  // Transmit buffer empty
#define SPI_SR_BSY   (1 << 7)  // Busy flag

// Static HAL instance
static sh2_Hal_t g_hal;

// Forward declarations
static uint32_t hal_getTimeUs(sh2_Hal_t *self);
static bool spihal_wait_for_int(void);
static void hal_hardwareReset(void);
static int spihal_open(sh2_Hal_t *self);
static void spihal_close(sh2_Hal_t *self);
static int spihal_read(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us);
static int spihal_write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len);

// Initialize SPI1 for BNO085 communication
static void SPI1_Init(void) {
    // Enable SPI1 clock (APB2ENR bit 12)
    RCC->APB2ENR |= (1 << 12);
    
    // Enable GPIOB clock for SPI pins
    RCC->AHB2ENR |= (1 << 1);  // GPIOB
    
    // Small delay for clock stabilization
    volatile int delay = 10;
    while (delay-- > 0) {
        __asm("nop");
    }
    
    // Configure PB3 (SCK) - Alternate function mode, AF5 for SPI1
    GPIOB->MODER &= ~(0b11 << (2 * SPI1_SCK_PIN));
    GPIOB->MODER |= (0b10 << (2 * SPI1_SCK_PIN));  // Alternate function
    GPIOB->AFRL &= ~(0b1111 << (4 * SPI1_SCK_PIN));
    GPIOB->AFRL |= (0b0101 << (4 * SPI1_SCK_PIN));  // AF5 = SPI1_SCK
    GPIOB->OSPEEDR |= (0b11 << (2 * SPI1_SCK_PIN));  // High speed
    
    // Configure PB5 (MOSI) - Alternate function mode, AF5 for SPI1
    GPIOB->MODER &= ~(0b11 << (2 * SPI1_MOSI_PIN));
    GPIOB->MODER |= (0b10 << (2 * SPI1_MOSI_PIN));  // Alternate function
    GPIOB->AFRL &= ~(0b1111 << (4 * SPI1_MOSI_PIN));
    GPIOB->AFRL |= (0b0101 << (4 * SPI1_MOSI_PIN));  // AF5 = SPI1_MOSI
    GPIOB->OSPEEDR |= (0b11 << (2 * SPI1_MOSI_PIN));  // High speed
    
    // Configure PB4 (MISO) - Alternate function mode, AF5 for SPI1
    GPIOB->MODER &= ~(0b11 << (2 * SPI1_MISO_PIN));
    GPIOB->MODER |= (0b10 << (2 * SPI1_MISO_PIN));  // Alternate function
    GPIOB->AFRL &= ~(0b1111 << (4 * SPI1_MISO_PIN));
    GPIOB->AFRL |= (0b0101 << (4 * SPI1_MISO_PIN));  // AF5 = SPI1_MISO
    GPIOB->PURPDR |= (0b01 << (2 * SPI1_MISO_PIN));  // Pull-up
    
    // Reset SPI1
    RCC->APB2RSTR |= (1 << 12);
    volatile int reset_delay = 10;
    while (reset_delay-- > 0) {
        __asm("nop");
    }
    RCC->APB2RSTR &= ~(1 << 12);
    
    // Configure SPI1: Mode 3 (CPOL=1, CPHA=1), Master, 8-bit, 1MHz
    // CR1 register:
    // - CPHA = 1 (bit 0)
    // - CPOL = 1 (bit 1)
    // - MSTR = 1 (bit 2) - Master mode
    // - BR = 0b101 (bits 5:3) - fPCLK/32 = 80MHz/32 = 2.5MHz (closest to 1MHz)
    //   Actually, let's use BR = 0b110 (fPCLK/64 = 1.25MHz) or 0b111 (fPCLK/128 = 625kHz)
    //   Use 0b110 for ~1.25MHz (close enough to 1MHz)
    // - SPE = 1 (bit 6) - Enable SPI
    // - SSM = 1 (bit 9) - Software slave management
    // - SSI = 1 (bit 8) - Internal slave select
    
    SPI1->CR1 = 0;
    SPI1->CR1 |= (1 << 0);   // CPHA = 1
    SPI1->CR1 |= (1 << 1);   // CPOL = 1
    SPI1->CR1 |= (1 << 2);   // MSTR = 1 (Master)
    SPI1->CR1 |= (0b110 << 3); // BR = fPCLK/64 (~1.25MHz)
    SPI1->CR1 |= (1 << 6);   // SPE = 1 (Enable)
    SPI1->CR1 |= (1 << 8);   // SSI = 1
    SPI1->CR1 |= (1 << 9);   // SSM = 1 (Software slave management)
}

// Initialize GPIO pins for NRST, CS, WAKE, INT
// Per datasheet: WAKE must stay HIGH from before reset until after first H_INTN assertion
static void GPIO_Init(void) {
    // Enable GPIOA clock
    RCC->AHB2ENR |= (1 << 0);  // GPIOA
    
    volatile int delay = 10;
    while (delay-- > 0) {
        __asm("nop");
    }
    
    // Configure PA0 (NRST) - Output, push-pull, high speed, initially HIGH (inactive)
    // NRST is active low, so HIGH = inactive (sensor not reset)
    GPIOA->MODER &= ~(0b11 << (2 * BNO085_RST_PIN));
    GPIOA->MODER |= (0b01 << (2 * BNO085_RST_PIN));  // Output
    GPIOA->OTYPER &= ~(1 << BNO085_RST_PIN);  // Push-pull
    GPIOA->OSPEEDR |= (0b11 << (2 * BNO085_RST_PIN));  // High speed
    GPIOA->ODR |= (1 << BNO085_RST_PIN);  // Set NRST high (inactive, sensor not reset)
    
    // Configure PA1 (H_INTN) - Input, pull-up
    // H_INTN is active low interrupt from sensor
    GPIOA->MODER &= ~(0b11 << (2 * BNO085_INT_PIN));
    // MODER = 00 = Input (already cleared)
    GPIOA->PURPDR &= ~(0b11 << (2 * BNO085_INT_PIN));
    GPIOA->PURPDR |= (0b01 << (2 * BNO085_INT_PIN));  // Pull-up
    
    // Configure PA11 (CS) - Output, push-pull, high speed
    GPIOA->MODER &= ~(0b11 << (2 * BNO085_CS_PIN));
    GPIOA->MODER |= (0b01 << (2 * BNO085_CS_PIN));  // Output
    GPIOA->OTYPER &= ~(1 << BNO085_CS_PIN);  // Push-pull
    GPIOA->OSPEEDR |= (0b11 << (2 * BNO085_CS_PIN));  // High speed
    GPIOA->ODR |= (1 << BNO085_CS_PIN);  // Set CS high (inactive)
    
    // Configure PA12 (WAKE) - Output, push-pull, high speed, initially HIGH
    // WAKE must stay HIGH during initialization per datasheet Section 1.2.4
    // WAKE is only used to wake sensor from sleep, NOT for reset
    GPIOA->MODER &= ~(0b11 << (2 * BNO085_WAKE_PIN));
    GPIOA->MODER |= (0b01 << (2 * BNO085_WAKE_PIN));  // Output
    GPIOA->OTYPER &= ~(1 << BNO085_WAKE_PIN);  // Push-pull
    GPIOA->OSPEEDR |= (0b11 << (2 * BNO085_WAKE_PIN));  // High speed
    GPIOA->ODR |= (1 << BNO085_WAKE_PIN);  // Set WAKE high (inactive, must stay HIGH during init)
}

// Hardware reset function (static, used internally)
// Per datasheet Section 1.2.1: NRST (PA0) is the hardware reset pin (active low)
// Reset sequence: HIGH -> LOW (10ms) -> HIGH
// Note: WAKE pin must remain HIGH during reset (per Section 1.2.4)
static void hal_hardwareReset(void) {
    // Ensure WAKE stays HIGH (required per datasheet)
    GPIOA->ODR |= (1 << BNO085_WAKE_PIN);   // WAKE high
    
    // NRST reset sequence: drive LOW to reset, then HIGH to release
    GPIOA->ODR |= (1 << BNO085_RST_PIN);    // NRST high (ensure not reset)
    ms_delay(1);
    GPIOA->ODR &= ~(1 << BNO085_RST_PIN);  // NRST low (active, reset sensor)
    ms_delay(10);  // Hold reset for 10ms (datasheet minimum is 10ns, 10ms is safe)
    GPIOA->ODR |= (1 << BNO085_RST_PIN);    // NRST high (release reset)
    // Note: After reset, sensor needs t1 (90ms) + t2 (4ms) = ~94ms to initialize
    // This delay is handled in main.c after calling this function
}

// Public function for hardware reset (matches Adafruit library hardwareReset())
void BNO085_HardwareReset(void) {
    hal_hardwareReset();
}

// Wait for INT pin (PA1, H_INTN) to go low (data ready)
// Matches Adafruit library implementation
// Per datasheet: H_INTN is active low interrupt from sensor
// Per datasheet Section 1.2.4.1: Host should respond within 1/10 of fastest sensor period
// For 100Hz sensors, that's 1ms, so 500ms timeout is reasonable
// Per datasheet Section 6.5.3: Sensor needs ~94ms after reset, so 500ms is safe
static bool spihal_wait_for_int(void) {
    // Check INT pin state immediately first
    if (!(GPIOA->IDR & (1 << BNO085_INT_PIN))) {
        return true;  // H_INTN is already low (active, data ready)
    }
    
    // Wait up to 500ms (500 iterations * 1ms delay) - matches Adafruit library
    // Per datasheet: After reset, sensor asserts H_INTN within ~94ms
    // We wait up to 500ms to be safe
    for (int i = 0; i < 500; i++) {
        if (!(GPIOA->IDR & (1 << BNO085_INT_PIN))) {
            return true;  // H_INTN is low (active, data ready)
        }
        ms_delay(1);
    }
    
    // Timeout - sensor not responding
    // This indicates the sensor is not asserting INT, which could mean:
    // 1. PS1 is not HIGH (sensor not in SPI mode)
    // 2. Sensor not powered
    // 3. Sensor not connected
    // 4. Sensor damaged
    // Note: Adafruit library calls hal_hardwareReset() on timeout, but we don't
    // because it can cause issues if sensor is not connected or if reset is handled elsewhere
    
    return false;
}

// SPI transfer function (write and read simultaneously)
// Added timeout to prevent infinite hanging
static uint8_t SPI1_Transfer(uint8_t data) {
    // Wait for TX buffer to be empty (with timeout)
    uint32_t timeout = 10000;  // 10000 iterations should be plenty
    while (!(SPI1->SR & SPI_SR_TXE) && timeout > 0) {
        timeout--;
        __asm("nop");
    }
    if (timeout == 0) {
        // TX buffer never became empty - SPI may not be working
        return 0xFF;  // Return error value
    }
    
    // Clear RX buffer if it has stale data
    if (SPI1->SR & SPI_SR_RXNE) {
        volatile uint8_t dummy = SPI1->DR;
        (void)dummy;  // Read to clear
    }
    
    // Write data (this starts the SPI clock)
    SPI1->DR = data;
    
    // Wait for RX buffer to have data (with timeout)
    // Note: In SPI mode, writing to DR triggers clock generation
    // The sensor should respond on MISO during clock cycles
    timeout = 10000;
    while (!(SPI1->SR & SPI_SR_RXNE) && timeout > 0) {
        timeout--;
        __asm("nop");
    }
    if (timeout == 0) {
        // RX buffer never received data - sensor may not be responding
        // This could indicate: MISO not connected, sensor not powered, or sensor not responding
        return 0xFF;  // Return error value
    }
    
    // Read data
    return (uint8_t)SPI1->DR;
}

// HAL open function
// Matches Adafruit library: just wait for INT pin
// Hardware reset should be done BEFORE calling sh2_open()
// Per datasheet: After reset, sensor asserts H_INTN to indicate ready
// Note: shtp_open() doesn't check return value, so this error may be ignored
// But sh2_open() will timeout after 200ms if reset notification never arrives
static int spihal_open(sh2_Hal_t *self) {
    // Wait for INT pin - sensor should assert INT after reset
    // This indicates the sensor is ready and has sent a reset notification
    // Per datasheet Section 6.5.3: Sensor needs ~94ms after reset
    // We wait up to 500ms to be safe
    bool int_asserted = spihal_wait_for_int();
    
    if (!int_asserted) {
        // INT never asserted - sensor may not be responding
        // This could indicate: PS1 not HIGH, sensor not powered, or sensor not connected
        // Note: shtp_open() doesn't check return value, so sh2_open() will still be called
        // sh2_open() will timeout after 200ms if reset notification never arrives
        return -1;
    }
    
    // INT asserted - sensor is ready
    return 0;
}

// HAL close function
// NOTE: Do NOT disable SPI here - it may be called during initialization
// and we need SPI to remain enabled for communication
static void spihal_close(sh2_Hal_t *self) {
    // Do not disable SPI - keep it enabled for ongoing communication
    // The Adafruit library may call close() during initialization
    // but we need SPI to stay enabled
    // SPI1->CR1 &= ~(1 << 6);  // SPE = 0 - COMMENTED OUT
}

// HAL read function
// Per datasheet Section 1.2.4: H_INTN is asserted when data is ready
// Per datasheet Section 6.5.4: H_CSN deasserts H_INTN (CS high = INT deasserted)
// Matches Adafruit library: waits for INT before reading
// Note: If INT times out, sensor may not be responding or may be in wrong mode
static int spihal_read(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us) {
    uint16_t packet_size = 0;
    static uint32_t read_call_count = 0;
    static uint32_t int_detected_count = 0;
    static uint32_t last_debug_time = 0;
    
    read_call_count++;
    
    // Debug: Print first few calls and periodic status
    if (read_call_count <= 5 || read_call_count % 500 == 0) {
        uint32_t current_time = hal_getTimeUs(self);
        bool int_state = (GPIOA->IDR & (1 << BNO085_INT_PIN)) != 0;
        DEBUG_PRINT("[SPI Read] Call #");
        DEBUG_PRINT_INT(read_call_count);
        DEBUG_PRINT(" | INT=");
        DEBUG_PRINT(int_state ? "HIGH" : "LOW");
        DEBUG_PRINT(" | Time=");
        DEBUG_PRINT_INT(current_time / 1000);
        DEBUG_PRINT(" ms");
        if (read_call_count > 5) {
            DEBUG_PRINT(" | Elapsed=");
            DEBUG_PRINT_INT((current_time - last_debug_time) / 1000);
            DEBUG_PRINT(" ms");
        }
        DEBUG_PRINT_NEWLINE();
        last_debug_time = current_time;
    }
    
    // Wait briefly for INT pin (data ready) - allows catching brief INT assertions
    // Per datasheet: H_INTN goes LOW when data is ready
    // Wait up to 20ms to catch brief INT assertions (non-blocking for main loop)
    bool int_asserted = false;
    for (int i = 0; i < 20; i++) {  // 20ms wait (20 iterations * 1ms)
        if (!(GPIOA->IDR & (1 << BNO085_INT_PIN))) {
            // INT is LOW (active) - data ready
            int_asserted = true;
            int_detected_count++;
            
            // Debug: Print first few INT detections and periodic updates
            if (int_detected_count <= 3 || int_detected_count % 100 == 0) {
                DEBUG_PRINT("[SPI Read] INT detected #");
                DEBUG_PRINT_INT(int_detected_count);
                DEBUG_PRINT(" (call #");
                DEBUG_PRINT_INT(read_call_count);
                DEBUG_PRINT(")");
                DEBUG_PRINT_NEWLINE();
            }
            break;
        }
        ms_delay(1);
    }
    
    if (!int_asserted) {
        // INT never asserted within 20ms - no data ready
        // Debug: Print periodic status (every 1000 calls)
        if (read_call_count % 1000 == 0) {
            DEBUG_PRINT("[SPI Read] Call #");
            DEBUG_PRINT_INT(read_call_count);
            DEBUG_PRINT(" - INT still HIGH (no data)");
            DEBUG_PRINT_NEWLINE();
        }
        return 0;
    }
    
    // INT is LOW (active) - data ready, proceed to read
    // Debug: Log SPI transaction start
    if (int_detected_count <= 3) {
        DEBUG_PRINT("[SPI Read] Starting SPI transaction (INT detected)");
        DEBUG_PRINT_NEWLINE();
    }
    
    // Pull CS low to start transaction
    // Per datasheet: CS deasserts H_INTN, so INT will go high when CS goes low
    // Per datasheet Section 6.5.2: tcssu (CS setup to CLK) = 0.1us minimum
    GPIOA->BSRR = (1 << (BNO085_CS_PIN + 16));  // Reset bit (set low)
    
    // Small delay for CS setup time (per datasheet: tcssu = 0.1us minimum)
    // Need enough delay for CS to settle before first clock edge
    // At 80MHz, 1 cycle = 12.5ns, so 10 cycles = 125ns (safe margin)
    volatile int cs_delay = 10;
    while (cs_delay-- > 0) {
        __asm("nop");
    }
    
    // Verify SPI is enabled and ready - if not, re-enable it
    uint32_t spi_cr1 = SPI1->CR1;
    bool spi_enabled = (spi_cr1 & (1 << 6)) != 0;  // SPE bit
    bool spi_master = (spi_cr1 & (1 << 2)) != 0;   // MSTR bit
    char hex_chars[] = "0123456789ABCDEF";  // Define outside if block for reuse
    
    if (int_detected_count <= 3) {
        uint32_t spi_sr = SPI1->SR;
        DEBUG_PRINT("[SPI Read] SPI CR1=0x");
        // Print hex manually to avoid format issues
        uint32_t val = spi_cr1;
        for (int i = 7; i >= 0; i--) {
            uint8_t nibble = (val >> (i * 4)) & 0xF;
            RTT_PrintChar(hex_chars[nibble]);
        }
        DEBUG_PRINT(" SR=0x");
        val = spi_sr;
        for (int i = 7; i >= 0; i--) {
            uint8_t nibble = (val >> (i * 4)) & 0xF;
            RTT_PrintChar(hex_chars[nibble]);
        }
        DEBUG_PRINT(" (SPE=");
        DEBUG_PRINT_INT(spi_enabled ? 1 : 0);
        DEBUG_PRINT(" MSTR=");
        DEBUG_PRINT_INT(spi_master ? 1 : 0);
        DEBUG_PRINT(")");
        DEBUG_PRINT_NEWLINE();
    }
    
    // If SPI is disabled, re-enable it
    if (!spi_enabled || !spi_master) {
        if (int_detected_count <= 3) {
            DEBUG_PRINT("[SPI Read] WARNING: SPI disabled! Re-enabling...");
            DEBUG_PRINT_NEWLINE();
        }
        // Re-enable SPI with correct configuration
        // On STM32L4, SPE must be cleared before modifying other bits
        // Per STM32L4 reference manual: CR1 can only be modified when SPE=0
        
        // First, disable SPI completely
        SPI1->CR1 &= ~(1 << 6);  // Clear SPE
        volatile int delay = 10;
        while (delay-- > 0) {
            __asm("nop");
        }
        
        // Wait for SPI to be fully disabled (check BSY bit in SR)
        uint32_t timeout = 1000;
        while ((SPI1->SR & (1 << 7)) && timeout > 0) {  // Wait for BSY to clear
            timeout--;
            __asm("nop");
        }
        
        // Now write all configuration bits at once (SPE still cleared)
        uint32_t cr1_value = 0;
        cr1_value |= (1 << 0);   // CPHA = 1
        cr1_value |= (1 << 1);   // CPOL = 1
        cr1_value |= (1 << 2);   // MSTR = 1 (Master)
        cr1_value |= (0b110 << 3); // BR = fPCLK/64 (~1.25MHz)
        cr1_value |= (1 << 8);   // SSI = 1
        cr1_value |= (1 << 9);   // SSM = 1 (Software slave management)
        // Note: SPE is NOT set yet
        
        SPI1->CR1 = cr1_value;
        
        // Small delay for configuration to settle
        delay = 10;
        while (delay-- > 0) {
            __asm("nop");
        }
        
        // Verify configuration was written (except SPE)
        uint32_t cr1_check = SPI1->CR1;
        if ((cr1_check & 0x3FF) != (cr1_value & 0x3FF)) {
            if (int_detected_count <= 3) {
                DEBUG_PRINT("[SPI Read] ERROR: CR1 write failed! Expected 0x");
                uint32_t val = cr1_value;
                for (int i = 7; i >= 0; i--) {
                    uint8_t nibble = (val >> (i * 4)) & 0xF;
                    RTT_PrintChar(hex_chars[nibble]);
                }
                DEBUG_PRINT(" got 0x");
                val = cr1_check;
                for (int i = 7; i >= 0; i--) {
                    uint8_t nibble = (val >> (i * 4)) & 0xF;
                    RTT_PrintChar(hex_chars[nibble]);
                }
                DEBUG_PRINT_NEWLINE();
            }
        }
        
        // Finally, enable SPI
        SPI1->CR1 |= (1 << 6);   // SPE = 1 (Enable)
        
        // Small delay for SPI to stabilize
        delay = 10;
        while (delay-- > 0) {
            __asm("nop");
        }
        
        // Verify it was actually enabled
        spi_cr1 = SPI1->CR1;
        spi_enabled = (spi_cr1 & (1 << 6)) != 0;
        spi_master = (spi_cr1 & (1 << 2)) != 0;
        
        if (int_detected_count <= 3) {
            DEBUG_PRINT("[SPI Read] SPI re-enabled. CR1=0x");
            uint32_t val = spi_cr1;
            for (int i = 7; i >= 0; i--) {
                uint8_t nibble = (val >> (i * 4)) & 0xF;
                RTT_PrintChar(hex_chars[nibble]);
            }
            DEBUG_PRINT(" (SPE=");
            DEBUG_PRINT_INT(spi_enabled ? 1 : 0);
            DEBUG_PRINT(" MSTR=");
            DEBUG_PRINT_INT(spi_master ? 1 : 0);
            DEBUG_PRINT(")");
            DEBUG_PRINT_NEWLINE();
        }
        
        if (!spi_enabled || !spi_master) {
            if (int_detected_count <= 3) {
                DEBUG_PRINT("[SPI Read] ERROR: Failed to enable SPI!");
                DEBUG_PRINT_NEWLINE();
            }
            return 0;  // Can't proceed without SPI
        }
    }
    
    // Pull CS low to start SPI transaction
    // CS will stay LOW for entire header read
    GPIOA->BSRR = (1 << (BNO085_CS_PIN + 16));  // Reset bit (set low)
    
    // Small delay for CS setup
    volatile int cs_delay = 10;
    while (cs_delay-- > 0) {
        __asm("nop");
    }
    
    // Read 4-byte SHTP header
    // Match Arduino: spi_dev->read(pBuffer, 4, 0x00)
    if (int_detected_count <= 3) {
        DEBUG_PRINT("[SPI Read] Reading header bytes...");
        DEBUG_PRINT_NEWLINE();
    }
    
    pBuffer[0] = SPI1_Transfer(0x00);
    pBuffer[1] = SPI1_Transfer(0x00);
    pBuffer[2] = SPI1_Transfer(0x00);
    pBuffer[3] = SPI1_Transfer(0x00);
    
    if (int_detected_count <= 3) {
        DEBUG_PRINT("[SPI Read] Header: 0x");
        char hex_chars[] = "0123456789ABCDEF";
        for (int i = 0; i < 4; i++) {
            uint8_t val = pBuffer[i];
            RTT_PrintChar(hex_chars[(val >> 4) & 0xF]);
            RTT_PrintChar(hex_chars[val & 0xF]);
        }
        DEBUG_PRINT_NEWLINE();
    }
    
    // Pull CS high to end header read
    // Per Arduino: spi_dev->read() handles CS automatically, so CS goes high after header
    GPIOA->BSRR = (1 << BNO085_CS_PIN);  // Set bit (set high)
    
    // Determine packet size from header (little-endian)
    // Match Arduino: packet_size = (uint16_t)pBuffer[0] | (uint16_t)pBuffer[1] << 8;
    packet_size = (uint16_t)pBuffer[0] | ((uint16_t)pBuffer[1] << 8);
    // Unset the "continue" bit
    packet_size &= ~0x8000;
    
    if (int_detected_count <= 3) {
        DEBUG_PRINT("[SPI Read] Packet size: ");
        DEBUG_PRINT_INT(packet_size);
        DEBUG_PRINT(" bytes, buffer size: ");
        DEBUG_PRINT_INT(len);
        DEBUG_PRINT_NEWLINE();
    }
    
    // Validate packet size
    if (packet_size < 4) {
        // Invalid packet size (too small)
        if (int_detected_count <= 3) {
            DEBUG_PRINT("[SPI Read] ERROR: Invalid packet size (too small): ");
            DEBUG_PRINT_INT(packet_size);
            DEBUG_PRINT_NEWLINE();
        }
        return 0;
    }
    
    // Match Arduino behavior: return 0 if packet doesn't fit in buffer
    // Let SHTP library handle fragmentation/retry
    if (packet_size > len) {
        if (int_detected_count <= 3) {
            DEBUG_PRINT("[SPI Read] Packet size ");
            DEBUG_PRINT_INT(packet_size);
            DEBUG_PRINT(" > buffer size ");
            DEBUG_PRINT_INT(len);
            DEBUG_PRINT(" - returning 0 (library will handle)");
            DEBUG_PRINT_NEWLINE();
        }
        return 0;
    }
    
    // If packet is exactly 4 bytes (header only), we're done
    if (packet_size == 4) {
        if (int_detected_count <= 3) {
            DEBUG_PRINT("[SPI Read] Header-only packet, returning");
            DEBUG_PRINT_NEWLINE();
        }
        if (t_us) {
            *t_us = hal_getTimeUs(self);
        }
        return packet_size;
    }
    
    // Wait for INT pin again for payload
    // Match Arduino: if (!spihal_wait_for_int()) return 0;
    if (int_detected_count <= 3) {
        DEBUG_PRINT("[SPI Read] Waiting for INT to read payload (");
        DEBUG_PRINT_INT(packet_size - 4);
        DEBUG_PRINT(" bytes)...");
        DEBUG_PRINT_NEWLINE();
    }
    
    if (!spihal_wait_for_int()) {
        if (int_detected_count <= 3) {
            DEBUG_PRINT("[SPI Read] ERROR: INT timeout waiting for payload");
            DEBUG_PRINT_NEWLINE();
        }
        return 0;
    }
    
    if (int_detected_count <= 3) {
        DEBUG_PRINT("[SPI Read] INT detected for payload, reading...");
        DEBUG_PRINT_NEWLINE();
    }
    
    // Pull CS low to start payload read
    // CS stays LOW for entire payload read - matching Arduino single-transaction approach
    GPIOA->BSRR = (1 << (BNO085_CS_PIN + 16));  // Reset bit (set low)
    
    // Small delay for CS setup
    cs_delay = 10;
    while (cs_delay-- > 0) {
        __asm("nop");
    }
    
    // Read payload (header already in buffer at indices 0-3, so read remaining bytes starting at index 4)
    // Match Arduino: spi_dev->read(pBuffer, packet_size, 0x00) - reads full packet including header
    // But we already have header, so we read from index 4 onwards
    // Keep CS LOW for entire payload read
    if (int_detected_count <= 3) {
        DEBUG_PRINT("[SPI Read] Reading ");
        DEBUG_PRINT_INT(packet_size - 4);
        DEBUG_PRINT(" payload bytes...");
        DEBUG_PRINT_NEWLINE();
    }
    for (unsigned i = 4; i < packet_size; i++) {
        pBuffer[i] = SPI1_Transfer(0x00);
        if (int_detected_count <= 3 && i < 8) {  // Print first 4 payload bytes
            DEBUG_PRINT("[SPI Read] Payload[");
            DEBUG_PRINT_INT(i);
            DEBUG_PRINT("] = 0x");
            uint8_t val = pBuffer[i];
            char hex_chars[] = "0123456789ABCDEF";
            RTT_PrintChar(hex_chars[(val >> 4) & 0xF]);
            RTT_PrintChar(hex_chars[val & 0xF]);
            DEBUG_PRINT_NEWLINE();
        }
    }
    
    // Pull CS high to end transaction
    GPIOA->BSRR = (1 << BNO085_CS_PIN);  // Set bit (set high)
    
    if (int_detected_count <= 3) {
        DEBUG_PRINT("[SPI Read] Complete! Returning ");
        DEBUG_PRINT_INT(packet_size);
        DEBUG_PRINT(" bytes");
        DEBUG_PRINT_NEWLINE();
    }
    
    // Set timestamp
    if (t_us) {
        *t_us = hal_getTimeUs(self);
    }
    
    return packet_size;
}

// HAL write function
static int spihal_write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len) {
    // Wait for INT pin (ready to receive)
    if (!spihal_wait_for_int()) {
        return 0;
    }
    
    // Pull CS low to start transaction
    GPIOA->BSRR = (1 << (BNO085_CS_PIN + 16));  // Reset bit (set low)
    
    // Write all bytes
    for (unsigned i = 0; i < len; i++) {
        SPI1_Transfer(pBuffer[i]);
    }
    
    // Pull CS high to end transaction
    GPIOA->BSRR = (1 << BNO085_CS_PIN);  // Set bit (set high)
    
    return len;
}

// HAL getTimeUs function
// Matches Adafruit library: millis() * 1000
// Uses SysTick for accurate millisecond timing

// SysTick registers (Cortex-M core peripheral)
#define SYSTICK_CTRL  (*((volatile uint32_t*)0xE000E010))
#define SYSTICK_LOAD  (*((volatile uint32_t*)0xE000E014))
#define SYSTICK_VAL   (*((volatile uint32_t*)0xE000E018))
#define SYSTICK_CALIB (*((volatile uint32_t*)0xE000E01C))

// Millisecond counter (incremented by SysTick interrupt)
static volatile uint32_t systick_ms_counter = 0;

// SysTick interrupt handler (called every 1ms)
void SysTick_Handler(void) {
    systick_ms_counter++;
}

// Initialize SysTick for 1ms interrupts
static void SysTick_Init_ms(void) {
    // Disable SysTick first
    SYSTICK_CTRL = 0;
    
    // Configure SysTick for 1ms ticks
    // System clock is 80MHz, so 1ms = 80000 cycles
    // LOAD register is 24-bit, so max value is 0xFFFFFF (16,777,215)
    // 80000 fits in 24 bits, so we use 80000 - 1 = 79999
    SYSTICK_LOAD = 79999;  // Reload value for 1ms (80000 cycles)
    SYSTICK_VAL = 0;       // Clear current value (writes to VAL clear it)
    
    // Configure SysTick:
    // Bit 2: CLKSOURCE = 1 (use processor clock, 80MHz)
    // Bit 1: TICKINT = 1 (enable interrupt)
    // Bit 0: ENABLE = 1 (enable counter)
    SYSTICK_CTRL = (1 << 2) | (1 << 1) | (1 << 0);
    
    systick_ms_counter = 0;
    
    // Enable interrupts globally (if not already enabled)
    // Use inline assembly for Cortex-M
    __asm volatile ("cpsie i" : : : "memory");
}

static uint32_t hal_getTimeUs(sh2_Hal_t *self) {
    // Return microseconds (milliseconds * 1000)
    // This matches library: millis() * 1000
    return systick_ms_counter * 1000;
}

// Initialize HAL structure
// This should be called BEFORE sh2_open()
// Matches Adafruit library: begin_SPI() sets up HAL, then _init() calls sh2_open()
int BNO085_SPI_HAL_Init(sh2_Hal_t *hal) {
    sh2_Hal_t *target_hal = hal;
    if (target_hal == NULL) {
        target_hal = &g_hal;
    }
    
    // Initialize SysTick first (needed for getTimeUs timing)
    SysTick_Init_ms();
    
    // Initialize SPI and GPIO (like library's begin_SPI does)
    SPI1_Init();
    GPIO_Init();
    
    // Set up HAL function pointers (matches library)
    target_hal->open = spihal_open;
    target_hal->close = spihal_close;
    target_hal->read = spihal_read;
    target_hal->write = spihal_write;
    target_hal->getTimeUs = hal_getTimeUs;
    
    return 0;
}

// Deinitialize HAL
void BNO085_SPI_HAL_DeInit(void) {
    // Disable SPI
    SPI1->CR1 &= ~(1 << 6);  // SPE = 0
}

