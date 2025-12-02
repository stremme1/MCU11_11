# LM386 Audio Amplifier Integration

## Hardware Connections

### LM386 Pin Configuration (8-pin PDIP)
```
Pin 1:  Gain Control (leave open for 20x gain)
Pin 2:  -Input (GND)
Pin 3:  +Input (from STM32L432KC PA0)
Pin 4:  GND
Pin 5:  Output (to Speaker +)
Pin 6:  VCC (4V-12V supply)
Pin 7:  Bypass (10μF to GND)
Pin 8:  Gain Control (leave open for 20x gain)
```

### Volume Control with Potentiometer
```
STM32L432KC PA0 ──── Potentiometer Wiper
                    │
                    Pot Pin 1 ──── LM386 Pin 3 (+Input)
                    Pot Pin 2 ──── GND  
                    Pot Pin 3 ──── GND
```

### Speaker Connection
```
LM386 Pin 5 (Output) ──── Speaker (+)
Speaker (-) ──── GND
```

## LM386 Specifications
- **Supply Voltage**: 4V-12V (perfect for 5V/9V battery)
- **Output Power**: 325mW into 8Ω load
- **Gain**: 20x (default) or 200x (with external cap)
- **Input Impedance**: 50kΩ
- **Quiescent Current**: 4mA
- **Low Distortion**: 0.2% THD

## Circuit Design
1. **Power Supply**: 5V-9V for LM386 (battery friendly!)
2. **Input Coupling**: Direct connection from PA0 via potentiometer
3. **Volume Control**: Potentiometer as voltage divider
4. **Output**: 8Ω speaker
5. **Bypass**: 10μF capacitor on Pin 7
6. **Gain Control**: Leave Pins 1 and 8 open for 20x gain (default)

## Benefits of LM386 Integration
- **Battery Operation**: 4V-12V supply range
- **Low Power**: Only 4mA quiescent current
- **Simple Design**: Minimal external components
- **Volume Control**: Hardware potentiometer control
- **Portable**: Perfect for battery-powered projects
- **No ADC Required**: Pure analog volume control
