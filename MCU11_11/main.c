// main.c
// Invisible Drum System for STM32L432KC
//
// Integrates single BNO085 sensor (right hand), drum hit detection, button inputs, and DAC audio playback

#include "STM32L432KC_RCC.h"
#include "STM32L432KC_GPIO.h"
#include "STM32L432KC_FLASH.h"
#include "STM32L432KC_DAC.h"
#include "STM32L432KC_TIMER.h"
#include "STM32L432KC_RTT.h"  // Debug RTT (Real-Time Transfer)
#include "BNO085_SPI_HAL.h"   // Provides BNO085_INT_PIN definition
#include "drum_detection.h"
#include "wav_arrays/drum_samples.h"
#include "sh2.h"
#include "sh2_SensorValue.h"
#include "sh2_err.h"  // For SH2_OK definition
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  // For NULL definition

// Button pin definitions
#define BUTTON1_PIN  6   // Button 1 - kick drum trigger
#define BUTTON2_PIN  7   // Button 2 - yaw offset reset/calibration

// Button debounce delays
#define DEBOUNCE_DELAY1  50  // ms
#define DEBOUNCE_DELAY2  50  // ms

// Button state variables
static uint32_t lastDebounceTime1 = 0;
static uint32_t lastDebounceTime2 = 0;
static bool buttonPrinted1 = false;
static bool buttonPrinted2 = false;

// Note: yawOffset is declared as extern in drum_detection.h and defined in drum_detection.c
// Do not redeclare it here

// Drum hit detection state
static DrumHitState_t drumState = {0};

// SH2 HAL instance
static sh2_Hal_t hal;

// Sensor value structure (shared with callback)
static sh2_SensorValue_t sensorValue;
static bool newSensorData = false;

// Sensor callback function
static void sensorHandler(void *cookie, sh2_SensorEvent_t *event) {
    // Decode sensor event
    sh2_decodeSensorEvent(&sensorValue, event);
    newSensorData = true;
    
    // Debug: Print detailed sensor data when it arrives
    static uint32_t sensor_data_count = 0;
    sensor_data_count++;
    
    // Print every sample with detailed information
    if (sensorValue.sensorId == SH2_GAME_ROTATION_VECTOR) {
        float q_real = sensorValue.un.gameRotationVector.real;
        float q_i = sensorValue.un.gameRotationVector.i;
        float q_j = sensorValue.un.gameRotationVector.j;
        float q_k = sensorValue.un.gameRotationVector.k;
        
        // Convert to Euler angles for display
        float roll, pitch, yaw;
        DrumDetection_QuaternionToEuler(q_real, q_i, q_j, q_k, &roll, &pitch, &yaw);
        yaw = DrumDetection_NormalizeYaw(yaw - yawOffset);
        
        DEBUG_PRINT("[Q #");
        DEBUG_PRINT_INT(sensor_data_count);
        DEBUG_PRINT("] r=");
        DEBUG_PRINT_FLOAT(q_real, 3);
        DEBUG_PRINT(" i=");
        DEBUG_PRINT_FLOAT(q_i, 3);
        DEBUG_PRINT(" j=");
        DEBUG_PRINT_FLOAT(q_j, 3);
        DEBUG_PRINT(" k=");
        DEBUG_PRINT_FLOAT(q_k, 3);
        DEBUG_PRINT(" | Roll=");
        DEBUG_PRINT_FLOAT(roll, 1);
        DEBUG_PRINT(" Pitch=");
        DEBUG_PRINT_FLOAT(pitch, 1);
        DEBUG_PRINT(" Yaw=");
        DEBUG_PRINT_FLOAT(yaw, 1);
        DEBUG_PRINT_NEWLINE();
    } 
    else if (sensorValue.sensorId == SH2_GYROSCOPE_CALIBRATED) {
        float gx = sensorValue.un.gyroscope.x;
        float gy = sensorValue.un.gyroscope.y;
        float gz = sensorValue.un.gyroscope.z;
        
        // Convert to approximate raw scale for display
        int16_t gyro_x_raw = (int16_t)(gx * 1000.0f);
        int16_t gyro_y_raw = (int16_t)(gy * 1000.0f);
        int16_t gyro_z_raw = (int16_t)(gz * 1000.0f);
        
        DEBUG_PRINT("[G #");
        DEBUG_PRINT_INT(sensor_data_count);
        DEBUG_PRINT("] x=");
        DEBUG_PRINT_FLOAT(gx, 3);
        DEBUG_PRINT(" y=");
        DEBUG_PRINT_FLOAT(gy, 3);
        DEBUG_PRINT(" z=");
        DEBUG_PRINT_FLOAT(gz, 3);
        DEBUG_PRINT(" | Raw: x=");
        DEBUG_PRINT_INT(gyro_x_raw);
        DEBUG_PRINT(" y=");
        DEBUG_PRINT_INT(gyro_y_raw);
        DEBUG_PRINT(" z=");
        DEBUG_PRINT_INT(gyro_z_raw);
        DEBUG_PRINT_NEWLINE();
    }
    else {
        // Other sensor types
        DEBUG_PRINT("[Sensor #");
        DEBUG_PRINT_INT(sensor_data_count);
        DEBUG_PRINT("] ID=");
        DEBUG_PRINT_INT(sensorValue.sensorId);
        DEBUG_PRINT_NEWLINE();
    }
}

// Function to get milliseconds
// Using a simple counter based on loop iterations
// For accurate timing, should use SysTick timer interrupt
static uint32_t get_millis(void) {
    static uint32_t ms_counter = 0;
    
    // Approximate: assume each main loop iteration takes ~1ms
    // This is very approximate - for production, use hardware timer
    ms_counter++;
    return ms_counter;
}

// Initialize buttons
static void Buttons_Init(void) {
    // Configure button pins as input with pull-up
    // Note: Using GPIOA for buttons (adjust pins as needed)
    RCC->AHB2ENR |= (1 << 0);  // Enable GPIOA clock
    
    volatile int delay = 10;
    while (delay-- > 0) {
        __asm("nop");
    }
    
    // Configure BUTTON1 (PA6) - Input with pull-up
    GPIOA->MODER &= ~(0b11 << (2 * BUTTON1_PIN));
    GPIOA->PURPDR &= ~(0b11 << (2 * BUTTON1_PIN));
    GPIOA->PURPDR |= (0b01 << (2 * BUTTON1_PIN));  // Pull-up
    
    // Configure BUTTON2 (PA7) - Input with pull-up
    GPIOA->MODER &= ~(0b11 << (2 * BUTTON2_PIN));
    GPIOA->PURPDR &= ~(0b11 << (2 * BUTTON2_PIN));
    GPIOA->PURPDR |= (0b01 << (2 * BUTTON2_PIN));  // Pull-up
}

// Read button state
// Buttons are active low (pulled up to 3.3V)
// Returns true when button is PRESSED (LOW), false when NOT pressed (HIGH)
static bool Button_Read(int pin) {
    return (GPIOA->IDR & (1 << pin)) == 0;  // Inverted: LOW = pressed
}

// Process button 1 (kick drum trigger)
static uint8_t ProcessButton1(void) {
    uint32_t currentTime = get_millis();
    
    if (currentTime - lastDebounceTime1 > DEBOUNCE_DELAY1) {
        bool reading = Button_Read(BUTTON1_PIN);
        
        if (reading && !buttonPrinted1) {
            buttonPrinted1 = true;
            lastDebounceTime1 = currentTime;
            DEBUG_PRINTLN("Button 1 pressed - KICK");
            return DRUM_KICK;
        }
        
        if (!reading) {
            buttonPrinted1 = false;
        }
        
        lastDebounceTime1 = currentTime;
    }
    
    return DRUM_NONE;
}

// Process button 2 (yaw offset reset)
static void ProcessButton2(void) {
    uint32_t currentTime = get_millis();
    
    if (currentTime - lastDebounceTime2 > DEBOUNCE_DELAY2) {
        bool reading = Button_Read(BUTTON2_PIN);
        
        if (reading && !buttonPrinted2) {
            buttonPrinted2 = true;
            lastDebounceTime2 = currentTime;
            
            // Read current quaternion and set yaw offset
            // This will be done when we get sensor data
            // For now, just reset the offset
            DrumDetection_SetYawOffset(0.0f);
            DEBUG_PRINTLN("Button 2 pressed - Yaw offset reset");
        }
        
        if (!reading) {
            buttonPrinted2 = false;
        }
        
        lastDebounceTime2 = currentTime;
    }
}

// Play drum sound based on ID
static void PlayDrumSound(uint8_t drumId) {
    const char* drumNames[] = {
        "SNARE", "HIHAT", "KICK", "HIGH_TOM", "MID_TOM", "CRASH", "RIDE", "LOW_TOM"
    };
    
    if (drumId < 8) {
        DEBUG_PRINT("Playing: ");
        DEBUG_PRINTLN(drumNames[drumId]);
    }
    
    switch (drumId) {
        case DRUM_SNARE:
            DAC_PlayWAV(snare_sample_data, snare_sample_length, snare_sample_sample_rate);
            break;
        case DRUM_HIHAT:
            DAC_PlayWAV(hihat_closed_sample_data, hihat_closed_sample_length, hihat_closed_sample_sample_rate);
            break;
        case DRUM_KICK:
            DAC_PlayWAV(kick_sample_data, kick_sample_length, kick_sample_sample_rate);
            break;
        case DRUM_HIGH_TOM:
            DAC_PlayWAV(tom_high_sample_data, tom_high_sample_length, tom_high_sample_sample_rate);
            break;
        case DRUM_MID_TOM:
            // Use mid tom or high tom (adjust as needed)
            DAC_PlayWAV(tom_high_sample_data, tom_high_sample_length, tom_high_sample_sample_rate);
            break;
        case DRUM_CRASH:
            DAC_PlayWAV(crash_sample_data, crash_sample_length, crash_sample_sample_rate);
            break;
        case DRUM_RIDE:
            DAC_PlayWAV(ride_sample_data, ride_sample_length, ride_sample_sample_rate);
            break;
        case DRUM_LOW_TOM:
            DAC_PlayWAV(tom_low_sample_data, tom_low_sample_length, tom_low_sample_sample_rate);
            break;
        default:
            DEBUG_PRINT("Unknown drum ID: ");
            DEBUG_PRINT_INT(drumId);
            DEBUG_PRINT_NEWLINE();
            break;
    }
}

int main(void) {
    // Initialize RTT for debug output first
    RTT_Init();
    ms_delay(10);  // Small delay for RTT initialization
    DEBUG_PRINTLN("=== Invisible Drum System Starting ===");
    
    // Initialize system
    DEBUG_PRINTLN("Initializing Flash...");
    configureFlash();
    DEBUG_PRINTLN("Flash configured");
    
    DEBUG_PRINTLN("Initializing Clock...");
    configureClock();
    DEBUG_PRINTLN("Clock configured");
    
    DEBUG_PRINTLN("System initialized");
    
    // Initialize DAC for audio output
    DEBUG_PRINTLN("Initializing DAC...");
    DAC_InitAudio(DAC_CHANNEL_1);
    DEBUG_PRINTLN("DAC initialized");
    
    // Initialize buttons
    DEBUG_PRINTLN("Initializing Buttons...");
    Buttons_Init();
    DEBUG_PRINTLN("Buttons initialized");
    
    // Initialize drum detection
    DEBUG_PRINTLN("Initializing Drum Detection...");
    DrumDetection_Init();
    DEBUG_PRINTLN("Drum detection initialized");
    
    // Initialize BNO085 SPI HAL (matches Adafruit library begin_SPI)
    DEBUG_PRINTLN("Initializing BNO085 SPI HAL...");
    BNO085_SPI_HAL_Init(&hal);
    DEBUG_PRINTLN("BNO085 SPI HAL initialized");
    
    // Hardware reset sensor BEFORE calling sh2_open() (matches Adafruit library _init)
    // Per datasheet Section 6.5.3: After reset, sensor needs:
    //   t1 (internal initialization) = 90ms typical
    //   t2 (internal configuration) = 4ms typical
    //   Total: ~94ms minimum, use 100ms for safety margin
    DEBUG_PRINTLN("Performing hardware reset...");
    BNO085_HardwareReset();
    DEBUG_PRINTLN("Hardware reset complete");
    
    // Wait for sensor initialization (datasheet requires ~94ms minimum)
    DEBUG_PRINTLN("Waiting for sensor initialization (100ms)...");
    ms_delay(100);  // Increased from 10ms to 100ms per datasheet requirements
    DEBUG_PRINTLN("Initialization delay complete");
    
    // Initialize SH2 protocol (matches Adafruit library: _init() calls sh2_open after reset)
    DEBUG_PRINTLN("Opening SH2 protocol...");
    DEBUG_PRINTLN("(Library will wait up to 200ms for sensor reset notification)");
    DEBUG_PRINTLN("Sensor should assert INT (go LOW) after reset");
    DEBUG_PRINTLN("If INT never asserts, check: PS1 HIGH, power, connections");
    
    // Check INT pin state before opening
    bool int_before = (GPIOA->IDR & (1 << BNO085_INT_PIN)) == 0;
    DEBUG_PRINT("INT pin before sh2_open(): ");
    DEBUG_PRINT(int_before ? "LOW (active)" : "HIGH (inactive)");
    DEBUG_PRINT_NEWLINE();
    
    // Get initial time for timeout detection
    uint32_t start_time = hal.getTimeUs(&hal);
    DEBUG_PRINT("Start time: ");
    DEBUG_PRINT_INT(start_time);
    DEBUG_PRINT_NEWLINE();
    
    DEBUG_PRINTLN("Calling sh2_open()...");
    DEBUG_PRINTLN("(This will wait for sensor to assert INT and send reset notification)");
    DEBUG_PRINTLN("(Timeout is 200ms - if sensor doesn't respond, will timeout after 200ms)");
    DEBUG_PRINTLN("(If hanging longer, check: PS1 HIGH, power, connections)");
    DEBUG_PRINTLN("(Monitoring: spihal_read() will be called repeatedly - watch for INT detection)");
    
    // Verify getTimeUs() is working by checking time before and after a small delay
    uint32_t time_check1 = hal.getTimeUs(&hal);
    ms_delay(10);
    uint32_t time_check2 = hal.getTimeUs(&hal);
    DEBUG_PRINT("Time check: ");
    DEBUG_PRINT_INT(time_check1);
    DEBUG_PRINT(" -> ");
    DEBUG_PRINT_INT(time_check2);
    DEBUG_PRINT(" (diff: ");
    DEBUG_PRINT_INT(time_check2 - time_check1);
    DEBUG_PRINT(" us, expected ~10000 us)");
    DEBUG_PRINT_NEWLINE();
    if ((time_check2 - time_check1) < 5000 || (time_check2 - time_check1) > 15000) {
        DEBUG_PRINTLN("WARNING: getTimeUs() may not be working correctly!");
    }
    
    int status = sh2_open(&hal, NULL, NULL);
    
    // Note: sh2_open() should timeout after 200ms (ADVERT_TIMEOUT_US)
    // If it hangs longer, there may be an issue with getTimeUs() or the timeout logic
    
    // Get end time
    uint32_t end_time = hal.getTimeUs(&hal);
    DEBUG_PRINT("End time: ");
    DEBUG_PRINT_INT(end_time);
    DEBUG_PRINT(" (elapsed: ");
    DEBUG_PRINT_INT(end_time - start_time);
    DEBUG_PRINT(" us = ");
    DEBUG_PRINT_INT((end_time - start_time) / 1000);
    DEBUG_PRINT(" ms)");
    DEBUG_PRINT_NEWLINE();
    
    // Check INT pin state after opening
    bool int_after = (GPIOA->IDR & (1 << BNO085_INT_PIN)) == 0;
    DEBUG_PRINT("INT pin after sh2_open(): ");
    DEBUG_PRINT(int_after ? "LOW (active)" : "HIGH (inactive)");
    DEBUG_PRINT_NEWLINE();
    
    if (status != SH2_OK) {
        DEBUG_PRINT("ERROR: SH2 open failed. Status: ");
        DEBUG_PRINT_INT(status);
        DEBUG_PRINT_NEWLINE();
        DEBUG_PRINTLN("Check: 1) Sensor power (3.3V) 2) SPI connections 3) CS/WAKE/INT pins");
        DEBUG_PRINTLN("Continuing without sensor - drum detection will not work.");
    } else {
        DEBUG_PRINTLN("SH2 protocol opened successfully!");
    }
    
    // Only configure sensors if SH2 opened successfully
    if (status == SH2_OK) {
        // Wait for sensor hub to be fully ready (advertisements complete, control channel set)
        // Per datasheet Section 5.2.1: After reset, sensor asserts H_INTN and sends SHTP advertisement
        // Service SH2 multiple times to ensure advertisements are processed
        DEBUG_PRINTLN("Waiting for sensor hub advertisements to complete...");
        DEBUG_PRINTLN("(Sensor should assert H_INTN and send advertisement packet)");
        DEBUG_PRINTLN("NOTE: PS1 pin must be HIGH (tied to VDDIO) for SPI mode per datasheet Section 1.2.4");
        
        // Check INT pin state for debugging
        bool int_state = (GPIOA->IDR & (1 << BNO085_INT_PIN)) != 0;
        DEBUG_PRINT("Initial INT pin state: ");
        DEBUG_PRINT(int_state ? "HIGH" : "LOW");
        DEBUG_PRINT_NEWLINE();
        
        // If INT is HIGH, sensor is not asserting interrupt
        // This could indicate: 1) PS1 not HIGH, 2) Sensor not powered, 3) Sensor not connected
        if (int_state) {
            DEBUG_PRINTLN("WARNING: INT pin is HIGH - sensor may not be asserting interrupt");
            DEBUG_PRINTLN("Check: 1) PS1 tied to VDDIO (3.3V) 2) Sensor power (3.3V) 3) SPI connections");
        }
        
        uint32_t read_count = 0;
        for (int i = 0; i < 200; i++) {  // Increased to 200 iterations (2 seconds)
            sh2_service();
            read_count++;
            
            // Debug: Print every 20 iterations
            if (i % 20 == 0) {
                int_state = (GPIOA->IDR & (1 << BNO085_INT_PIN)) != 0;
                DEBUG_PRINT("[Adv] Iteration ");
                DEBUG_PRINT_INT(i);
                DEBUG_PRINT(" INT=");
                DEBUG_PRINT(int_state ? "HIGH" : "LOW");
                DEBUG_PRINT_NEWLINE();
            }
            
            ms_delay(10);
        }
        DEBUG_PRINT("Advertisement processing complete (");
        DEBUG_PRINT_INT(read_count);
        DEBUG_PRINT(" service calls)");
        DEBUG_PRINT_NEWLINE();
        
        // Register sensor callback FIRST (before configuring sensors)
        DEBUG_PRINTLN("Registering sensor callback...");
        int callback_status = sh2_setSensorCallback(sensorHandler, NULL);
        if (callback_status != SH2_OK) {
            DEBUG_PRINT("ERROR: Failed to register sensor callback. Status: ");
            DEBUG_PRINT_INT(callback_status);
            DEBUG_PRINT_NEWLINE();
        } else {
            DEBUG_PRINTLN("Sensor callback registered");
        }
        
        // Service SH2 a few times after callback registration
        for (int i = 0; i < 10; i++) {
            sh2_service();
            ms_delay(10);
        }
        
        // Enable Game Rotation Vector reports (for quaternion)
        DEBUG_PRINTLN("Configuring Game Rotation Vector...");
        sh2_SensorConfig_t config;
        config.changeSensitivityEnabled = false;
        config.wakeupEnabled = false;
        config.changeSensitivityRelative = false;
        config.alwaysOnEnabled = false;
        config.changeSensitivity = 0;
        config.batchInterval_us = 0;
        config.sensorSpecific = 0;
        config.reportInterval_us = 10000;  // 10ms = 100Hz
        
        // Service SH2 before configuring to ensure ready
        sh2_service();
        ms_delay(10);
        
        int grv_status = sh2_setSensorConfig(SH2_GAME_ROTATION_VECTOR, &config);
        if (grv_status != SH2_OK) {
            DEBUG_PRINT("ERROR: Failed to configure Game Rotation Vector. Status: ");
            DEBUG_PRINT_INT(grv_status);
            DEBUG_PRINT(" (SH2_ERR_BAD_PARAM = -2)");
            DEBUG_PRINT_NEWLINE();
            DEBUG_PRINTLN("This may indicate sensor hub not ready or control channel not set");
        } else {
            DEBUG_PRINTLN("Game Rotation Vector configured");
        }
        
        // Service SH2 to process the configuration response
        for (int i = 0; i < 50; i++) {
            sh2_service();
            ms_delay(10);
        }
        
        // Enable Gyroscope reports
        DEBUG_PRINTLN("Configuring Gyroscope...");
        sh2_service();  // Service before config
        ms_delay(10);
        
        int gyro_status = sh2_setSensorConfig(SH2_GYROSCOPE_CALIBRATED, &config);
        if (gyro_status != SH2_OK) {
            DEBUG_PRINT("ERROR: Failed to configure Gyroscope. Status: ");
            DEBUG_PRINT_INT(gyro_status);
            DEBUG_PRINT(" (SH2_ERR_BAD_PARAM = -2)");
            DEBUG_PRINT_NEWLINE();
            DEBUG_PRINTLN("This may indicate sensor hub not ready or control channel not set");
        } else {
            DEBUG_PRINTLN("Gyroscope configured");
        }
        
        // Service SH2 to process the configuration response
        for (int i = 0; i < 50; i++) {
            sh2_service();
            ms_delay(10);
        }
        
        // Give sensor a moment to start sending data after configuration
        DEBUG_PRINTLN("Waiting for sensor to start sending data...");
        ms_delay(200);  // Increased delay to allow sensor to start
        
        // Service SH2 multiple times to process any initial data
        DEBUG_PRINTLN("Processing initial sensor data...");
        for (int i = 0; i < 100; i++) {
            sh2_service();
            ms_delay(10);
        }
        DEBUG_PRINTLN("Initial SH2 service calls complete");
    } else {
        DEBUG_PRINTLN("Skipping sensor configuration (SH2 not opened)");
    }
    
    DEBUG_PRINTLN("=== System Ready - Entering Main Loop ===");
    
    // Main loop
    static uint32_t loop_count = 0;
    static uint32_t sh2_service_count = 0;
    
    DEBUG_PRINTLN("Entering main loop...");
    
    while (1) {
        loop_count++;
        
        // Service SH2 protocol (must be called regularly) - only if opened successfully
        if (status == SH2_OK) {
            sh2_service();
            sh2_service_count++;
            
            // Debug: Print first few service calls and then periodically
            if (sh2_service_count <= 10 || sh2_service_count % 1000 == 0) {
                DEBUG_PRINT("[SH2] Service call #");
                DEBUG_PRINT_INT(sh2_service_count);
                DEBUG_PRINT_NEWLINE();
            }
        }
        
        // Check for new sensor data
        if (newSensorData) {
            newSensorData = false;
            
            // Periodic debug output for sensor values (every 1000 samples)
            static uint32_t sensor_debug_count = 0;
            sensor_debug_count++;
            if (sensor_debug_count % 1000 == 0) {
                if (sensorValue.sensorId == SH2_GAME_ROTATION_VECTOR) {
                    float q_real = sensorValue.un.gameRotationVector.real;
                    float q_i = sensorValue.un.gameRotationVector.i;
                    float q_j = sensorValue.un.gameRotationVector.j;
                    float q_k = sensorValue.un.gameRotationVector.k;
                    DEBUG_PRINT("Quaternion: r=");
                    DEBUG_PRINT_FLOAT(q_real, 3);
                    DEBUG_PRINT(" i=");
                    DEBUG_PRINT_FLOAT(q_i, 3);
                    DEBUG_PRINT(" j=");
                    DEBUG_PRINT_FLOAT(q_j, 3);
                    DEBUG_PRINT(" k=");
                    DEBUG_PRINT_FLOAT(q_k, 3);
                    DEBUG_PRINT_NEWLINE();
                } else if (sensorValue.sensorId == SH2_GYROSCOPE_CALIBRATED) {
                    float gx = sensorValue.un.gyroscope.x;
                    float gy = sensorValue.un.gyroscope.y;
                    float gz = sensorValue.un.gyroscope.z;
                    DEBUG_PRINT("Gyro: x=");
                    DEBUG_PRINT_FLOAT(gx, 3);
                    DEBUG_PRINT(" y=");
                    DEBUG_PRINT_FLOAT(gy, 3);
                    DEBUG_PRINT(" z=");
                    DEBUG_PRINT_FLOAT(gz, 3);
                    DEBUG_PRINT_NEWLINE();
                }
            }
            
            // Process sensor data for drum detection
            uint8_t drumId = DrumDetection_ProcessSensorData(&sensorValue, &drumState);
            if (drumId != DRUM_NONE) {
                PlayDrumSound(drumId);
            }
        }
        
        // Process buttons
        uint8_t buttonDrum = ProcessButton1();
        if (buttonDrum != DRUM_NONE) {
            PlayDrumSound(buttonDrum);
        }
        
        ProcessButton2();
        
        // Periodic status update (every 10000 loops ~ every 10 seconds at 1ms delay)
        if (loop_count % 10000 == 0) {
            DEBUG_PRINT("Loop count: ");
            DEBUG_PRINT_INT(loop_count);
            DEBUG_PRINT_NEWLINE();
        }
        
        // Small delay to prevent tight loop
        // Note: DAC_PlayWAV is blocking, so this delay mainly affects sensor reading rate
        ms_delay(1);
    }
    
    return 0;
}
