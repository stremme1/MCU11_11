# BNO085 Hardware Troubleshooting Guide

## Current Issue: INT Pin Stays HIGH - Sensor Not Responding

**CRITICAL**: The sensor is not asserting the H_INTN interrupt pin at all. This prevents:
- Reset notification from being received (causes `sh2_open()` to hang)
- Advertisement packets from being received
- Sensor configuration (control channel not set)
- Sensor data from being received

**Symptom**: `sh2_open()` hangs indefinitely (should timeout after 200ms but doesn't complete)

## Hardware Requirements (Per Datasheet Section 1.2.4)

### Critical: PS1 Pin Must Be HIGH for SPI Mode

**PS1 (Pin 5) must be tied to VDDIO (3.3V) for SPI mode.**

- If PS1 is LOW or floating, the sensor will NOT operate in SPI mode
- PS1 is sampled at reset, so it must be HIGH before and during reset
- PS1 can be tied directly to VDDIO (3.3V) - no GPIO control needed

### Power Requirements

- **VDD**: 2.4V to 3.6V (sensor power supply)
- **VDDIO**: 1.65V to 3.6V (I/O domain power supply)
- Both must be powered and stable before reset

### Pin Configuration for SPI Mode

| BNO085 Pin | STM32L432KC Pin | Function | State Required |
|------------|-----------------|----------|----------------|
| PS1 (Pin 5) | **VDDIO (3.3V)** | Protocol Select 1 | **MUST BE HIGH** |
| PS0/WAKE (Pin 6) | PA12 | Protocol Select 0 / Wake | HIGH (during init) |
| NRST (Pin 11) | PA0 | Reset (active low) | HIGH (inactive) |
| H_INTN (Pin 14) | PA1 | Interrupt (active low) | Input, Pull-up |
| H_CSN (Pin 18) | PA11 | Chip Select (active low) | Output, HIGH (inactive) |
| SCK (Pin 19) | PB3 | SPI Clock | SPI1_SCK |
| MOSI (Pin 17) | PB5 | SPI Data Out | SPI1_MOSI |
| MISO (Pin 20) | PB4 | SPI Data In | SPI1_MISO |

### Reset Sequence (Per Datasheet Section 6.5.3)

1. **Before Reset**: PS1 and PS0 must both be HIGH
2. **Reset**: Drive NRST LOW for at least 10ns (we use 10ms for safety)
3. **After Reset**: Wait for H_INTN to assert (sensor sends reset notification)
4. **Timing**: Sensor needs ~94ms (t1=90ms + t2=4ms) to initialize after reset

## Diagnostic Steps

### 1. Verify PS1 is HIGH

**CRITICAL**: Use a multimeter or oscilloscope to verify PS1 (Pin 5) is at 3.3V.

- If PS1 is LOW or floating, the sensor will NOT work in SPI mode
- PS1 must be HIGH before, during, and after reset

### 2. Verify Power

- Check VDD is 3.3V (or within 2.4V-3.6V range)
- Check VDDIO is 3.3V (or within 1.65V-3.6V range)
- Verify GND connections are solid

### 3. Verify SPI Connections

- SCK: PB3 → BNO085 Pin 19
- MOSI: PB5 → BNO085 Pin 17
- MISO: PB4 → BNO085 Pin 20
- CS: PA11 → BNO085 Pin 18
- INT: PA1 → BNO085 Pin 14
- WAKE: PA12 → BNO085 Pin 6
- NRST: PA0 → BNO085 Pin 11

### 4. Check INT Pin Behavior

- INT should be HIGH (inactive) when no data is ready
- INT should go LOW (active) when sensor has data
- After `sh2_open()` succeeds, INT should go LOW for advertisement packets
- If INT stays HIGH, sensor is not asserting interrupt

### 5. Verify Reset Sequence

- NRST should be HIGH (inactive) normally
- During reset: NRST goes LOW for 10ms, then HIGH
- After reset: Wait 100ms+ for sensor to initialize
- Sensor should assert INT after reset (for reset notification)

## Common Issues

### Issue: INT Pin Stays HIGH - Sensor Not Asserting Interrupt

**Symptoms**: 
- `sh2_open()` hangs (never completes, should timeout after 200ms)
- INT pin stays HIGH (never goes LOW)
- Sensor never sends reset notification
- No advertisements received
- Sensor configuration fails with `SH2_ERR_BAD_PARAM`

**Possible Causes** (in order of likelihood):
1. **PS1 is not HIGH** - **MOST LIKELY CAUSE!** PS1 must be tied to VDDIO (3.3V) for SPI mode
2. Sensor not powered correctly (VDD and/or VDDIO not connected or wrong voltage)
3. SPI connections incorrect (SCK, MOSI, MISO, CS, INT, WAKE, NRST)
4. Sensor damaged or not connected
5. Sensor in bad state (try power cycle)

**Solutions** (try in this order):
1. **VERIFY PS1 is tied to VDDIO (3.3V)** - Use multimeter to check PS1 (Pin 5) is at 3.3V
   - If PS1 is LOW or floating, sensor will NOT work in SPI mode
   - This is the most common cause of this issue
2. Check all power connections:
   - VDD: 3.3V (or 2.4V-3.6V range)
   - VDDIO: 3.3V (or 1.65V-3.6V range)
   - GND: Connected
3. Verify SPI pin connections (see Pin Configuration table above)
4. Try power cycling the sensor:
   - Remove power completely
   - Wait 1 second
   - Reapply power
   - Try again
5. Check sensor with multimeter/oscilloscope:
   - Verify INT pin can be read (should be HIGH normally)
   - Verify CS pin can be controlled
   - Verify SPI clock is working

### Issue: `sh2_open()` Times Out

**Symptoms**:
- `sh2_open()` takes > 1 second or times out
- INT never goes LOW

**Possible Causes**:
1. Sensor not powered
2. NRST not connected or not working
3. Sensor damaged
4. PS1 not HIGH (sensor in wrong mode)

**Solutions**:
1. Check power connections
2. Verify NRST is connected and working
3. Try power cycling
4. Verify PS1 is HIGH

## Expected Behavior

### Successful Initialization Sequence

1. Hardware reset: NRST LOW → HIGH
2. Wait 100ms for sensor initialization
3. `sh2_open()` called
4. Sensor asserts INT (goes LOW)
5. `spihal_wait_for_int()` detects INT LOW
6. Read reset notification packet
7. Sensor deasserts INT (goes HIGH)
8. Sensor asserts INT again for advertisement packet
9. Read advertisement packet
10. `controlChan` gets set (no longer 0xFF)
11. Sensor configuration succeeds

### Current Behavior (Problem)

1. Hardware reset: NRST LOW → HIGH ✓
2. Wait 100ms ✓
3. `sh2_open()` called ✓
4. Sensor asserts INT (goes LOW) ✓ (takes 651ms, so it happened)
5. Read reset notification packet ✓
6. Sensor deasserts INT (goes HIGH) ✓
7. **Sensor does NOT assert INT again** ✗
8. No advertisement packet received ✗
9. `controlChan` stays 0xFF ✗
10. Sensor configuration fails ✗

## Next Steps

1. **Verify PS1 is HIGH** - Use multimeter to check PS1 (Pin 5) is at 3.3V
2. **Check power** - Verify VDD and VDDIO are both 3.3V
3. **Verify connections** - Double-check all SPI pins are connected correctly
4. **Try power cycle** - Remove power for 1 second, then reapply
5. **Check sensor** - Verify sensor is not damaged

If PS1 is confirmed HIGH and power is correct, the sensor may be in a bad state or there may be a connection issue.

