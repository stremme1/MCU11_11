# Pin Configuration Verification

## BNO085 Sensor Pin Mapping

| BNO085 Pin | STM32L432KC Pin | Function | Configuration | Status |
|------------|-----------------|----------|---------------|--------|
| VIN        | 3.3V            | Power    | Power supply  | ✓ Correct |
| GND        | GND             | Ground   | Common ground | ✓ Correct |
| SCK        | PB3             | SPI Clock | SPI1_SCK, AF5, High speed | ✓ Correct |
| MOSI       | PB5             | SPI Data Out | SPI1_MOSI, AF5, High speed | ✓ Correct |
| MISO       | PB4             | SPI Data In | SPI1_MISO, AF5, Pull-up | ✓ Correct |
| CS         | PA11            | Chip Select | GPIO Output, Push-pull, Active low | ✓ Correct |
| PS0/WAKE   | PA12            | Wake Signal | GPIO Output, Push-pull, Active low | ✓ Correct |
| INT        | PA15            | Interrupt | GPIO Input, Pull-up, Active low | ✓ Correct |

## Implementation Details

### SPI1 Configuration
- **Mode**: SPI Mode 3 (CPOL=1, CPHA=1)
- **Clock Speed**: ~1.25MHz (fPCLK/64, where fPCLK = 80MHz)
- **Data Format**: 8-bit
- **Master Mode**: Enabled
- **Software Slave Management**: Enabled (SSM=1, SSI=1)

### GPIO Configuration

#### SPI Pins (GPIOB)
- **PB3 (SCK)**: Alternate Function 5, High speed
- **PB5 (MOSI)**: Alternate Function 5, High speed  
- **PB4 (MISO)**: Alternate Function 5, Pull-up enabled

#### Control Pins (GPIOA)
- **PA11 (CS)**: Output mode, Push-pull, High speed, Initially HIGH (inactive)
- **PA12 (WAKE)**: Output mode, Push-pull, High speed, Initially HIGH (inactive)
- **PA15 (INT)**: Input mode, Pull-up enabled

### Code References
- Pin definitions: `BNO085_SPI_HAL.h` (lines 13-20)
- SPI initialization: `BNO085_SPI_HAL.c` (lines 40-102)
- GPIO initialization: `BNO085_SPI_HAL.c` (lines 105-133)

## Additional Pins

### DAC Audio Output
- **PA4**: DAC Channel 1 output (analog mode)

### Buttons
- **PA6**: Button 1 (Kick drum trigger) - Input with pull-up
- **PA7**: Button 2 (Yaw offset reset) - Input with pull-up

## Notes
- All pin configurations match the specified requirements
- CS and WAKE pins are active low (pulled low to activate)
- INT pin is active low (goes low when data is ready)
- SPI communication uses proper CS toggling for each transaction

