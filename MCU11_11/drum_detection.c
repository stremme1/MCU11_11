// drum_detection.c
// Drum hit detection logic implementation

#include "drum_detection.h"
#include "STM32L432KC_RTT.h"  // For debug output (RTT)
#include <math.h>
#include <stddef.h>  // For NULL definition

// Define M_PI if not defined by math.h (some embedded toolchains don't define it)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Yaw offset for calibration
float yawOffset = 0.0f;

// Convert quaternion to Euler angles (roll, pitch, yaw in degrees)
void DrumDetection_QuaternionToEuler(float q_real, float q_i, float q_j, float q_k, 
                                     float *roll, float *pitch, float *yaw) {
    // Convert quaternion to Euler angles
    // Using standard conversion formulas
    
    // Roll (x-axis rotation)
    float sinr_cosp = 2.0f * (q_real * q_i + q_j * q_k);
    float cosr_cosp = 1.0f - 2.0f * (q_i * q_i + q_j * q_j);
    *roll = atan2f(sinr_cosp, cosr_cosp) * 180.0f / M_PI;
    
    // Pitch (y-axis rotation)
    float sinp = 2.0f * (q_real * q_j - q_k * q_i);
    if (fabsf(sinp) >= 1.0f) {
        *pitch = copysignf(M_PI / 2.0f, sinp) * 180.0f / M_PI;  // Use 90 degrees if out of range
    } else {
        *pitch = asinf(sinp) * 180.0f / M_PI;
    }
    
    // Yaw (z-axis rotation)
    float siny_cosp = 2.0f * (q_real * q_k + q_i * q_j);
    float cosy_cosp = 1.0f - 2.0f * (q_j * q_j + q_k * q_k);
    *yaw = atan2f(siny_cosp, cosy_cosp) * 180.0f / M_PI;
}

// Normalize yaw to 0-360 range
float DrumDetection_NormalizeYaw(float yaw) {
    yaw = fmodf(yaw, 360.0f);
    if (yaw < 0.0f) {
        yaw += 360.0f;
    }
    return yaw;
}

// Set yaw offset for calibration
void DrumDetection_SetYawOffset(float offset) {
    yawOffset = offset;
}

// Initialize drum detection
void DrumDetection_Init(void) {
    yawOffset = 0.0f;
}

// Process sensor data and detect drum hits
// Returns drum sound ID if hit detected, DRUM_NONE otherwise
uint8_t DrumDetection_ProcessSensorData(sh2_SensorValue_t *sensorValue, DrumHitState_t *state) {
    if (sensorValue == NULL || state == NULL) {
        return DRUM_NONE;
    }
    
    static float last_yaw = 0.0f;
    static float last_pitch = 0.0f;
    static int16_t last_gyro_y = 0;
    
    // Check if we have Game Rotation Vector data
    if (sensorValue->sensorId == SH2_GAME_ROTATION_VECTOR) {
        // Extract quaternion
        float q_real = sensorValue->un.gameRotationVector.real;
        float q_i = sensorValue->un.gameRotationVector.i;
        float q_j = sensorValue->un.gameRotationVector.j;
        float q_k = sensorValue->un.gameRotationVector.k;
        
        // Convert to Euler angles
        float roll, pitch, yaw;
        DrumDetection_QuaternionToEuler(q_real, q_i, q_j, q_k, &roll, &pitch, &yaw);
        
        // Adjust yaw by offset and normalize
        yaw = DrumDetection_NormalizeYaw(yaw - yawOffset);
        
        last_yaw = yaw;
        last_pitch = pitch;
    }
    
    // Check if we have Gyroscope data
    if (sensorValue->sensorId == SH2_GYROSCOPE_CALIBRATED) {
        // Extract gyroscope data (in rad/s, convert to similar scale as original)
        // Original code used raw gyro values, BNO085 gives calibrated in rad/s
        // Convert rad/s to approximate raw scale: multiply by ~1000
        float gyro_y_rads = sensorValue->un.gyroscope.y;
        int16_t gyro_y = (int16_t)(gyro_y_rads * 1000.0f);  // Approximate conversion
        
        last_gyro_y = gyro_y;
        
        // Debug: Always show gyro_y value and threshold comparison
        static uint32_t gyro_debug_count = 0;
        gyro_debug_count++;
        if (gyro_debug_count % 10 == 0) {  // Print every 10th sample to avoid spam
            RTT_PrintStr("[Gyro Check] gyro_y=");
            RTT_PrintInt(gyro_y);
            RTT_PrintStr(" threshold=");
            RTT_PrintInt(GYRO_HIT_THRESHOLD);
            RTT_PrintStr(" (");
            RTT_PrintInt(gyro_y < GYRO_HIT_THRESHOLD ? 1 : 0);
            RTT_PrintStr(") | Yaw=");
            RTT_PrintFloat(last_yaw, 1);
            RTT_PrintStr(" Pitch=");
            RTT_PrintFloat(last_pitch, 1);
            RTT_PrintNewline();
        }
        
        // Hit detection logic for single sensor (right hand)
        // Check if gyro_y indicates a hit
        if (gyro_y < GYRO_HIT_THRESHOLD && !state->printedForGyro) {
            state->hitDetected = true;
            state->printedForGyro = true;
            
            // Enhanced debug output
            RTT_PrintStr("*** HIT DETECTED *** Gyro_y: ");
            RTT_PrintInt(gyro_y);
            RTT_PrintStr(" (threshold: ");
            RTT_PrintInt(GYRO_HIT_THRESHOLD);
            RTT_PrintStr(") | Yaw: ");
            RTT_PrintFloat(last_yaw, 1);
            RTT_PrintStr(" Pitch: ");
            RTT_PrintFloat(last_pitch, 1);
            RTT_PrintStr(" -> ");
            
            // Determine which drum based on yaw angle
            // Single sensor (right hand) zone mapping
            if (last_yaw >= 20.0f && last_yaw <= 120.0f) {
                // Snare drum
                state->lastDrumSound = DRUM_SNARE;
                RTT_PrintStr("SNARE (yaw zone: 20-120)");
                RTT_PrintNewline();
                return DRUM_SNARE;
            }
            else if (last_yaw >= 340.0f || last_yaw <= 20.0f) {
                // High tom or crash cymbal
                if (last_pitch > 50.0f) {
                    state->lastDrumSound = DRUM_CRASH;
                    RTT_PrintStr("CRASH (yaw: 340-360/0-20, pitch>50)");
                    RTT_PrintNewline();
                    return DRUM_CRASH;
                } else {
                    state->lastDrumSound = DRUM_HIGH_TOM;
                    RTT_PrintStr("HIGH_TOM (yaw: 340-360/0-20, pitch<=50)");
                    RTT_PrintNewline();
                    return DRUM_HIGH_TOM;
                }
            }
            else if (last_yaw >= 305.0f && last_yaw <= 340.0f) {
                // Mid tom or ride cymbal
                if (last_pitch > 50.0f) {
                    state->lastDrumSound = DRUM_RIDE;
                    RTT_PrintStr("RIDE (yaw: 305-340, pitch>50)");
                    RTT_PrintNewline();
                    return DRUM_RIDE;
                } else {
                    state->lastDrumSound = DRUM_MID_TOM;
                    RTT_PrintStr("MID_TOM (yaw: 305-340, pitch<=50)");
                    RTT_PrintNewline();
                    return DRUM_MID_TOM;
                }
            }
            else if (last_yaw >= 200.0f && last_yaw <= 305.0f) {
                // Floor tom or ride cymbal
                if (last_pitch > 30.0f) {
                    state->lastDrumSound = DRUM_RIDE;
                    RTT_PrintStr("RIDE (yaw: 200-305, pitch>30)");
                    RTT_PrintNewline();
                    return DRUM_RIDE;
                } else {
                    state->lastDrumSound = DRUM_LOW_TOM;
                    RTT_PrintStr("LOW_TOM (yaw: 200-305, pitch<=30)");
                    RTT_PrintNewline();
                    return DRUM_LOW_TOM;
                }
            }
            else {
                RTT_PrintStr("UNKNOWN ZONE (yaw=");
                RTT_PrintFloat(last_yaw, 1);
                RTT_PrintStr(")");
                RTT_PrintNewline();
            }
        } else if (gyro_y >= GYRO_HIT_THRESHOLD && state->printedForGyro) {
            // Reset debounce flag when gyro returns to normal
            state->printedForGyro = false;
            state->hitDetected = false;
        }
    }
    
    return DRUM_NONE;
}

