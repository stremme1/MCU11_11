// drum_detection.h
// Drum hit detection logic for invisible drum system
//
// Detects drum hits based on single BNO085 sensor data (right hand)
// Uses quaternion (Game Rotation Vector) and gyroscope data

#ifndef DRUM_DETECTION_H
#define DRUM_DETECTION_H

#include <stdint.h>
#include <stdbool.h>
#include "sh2_SensorValue.h"
#include "sh2.h"  // For SH2_GAME_ROTATION_VECTOR and SH2_GYROSCOPE_CALIBRATED definitions

// Drum sound IDs (matching original code)
#define DRUM_SNARE       0
#define DRUM_HIHAT       1
#define DRUM_KICK        2
#define DRUM_HIGH_TOM    3
#define DRUM_MID_TOM     4
#define DRUM_CRASH       5
#define DRUM_RIDE        6
#define DRUM_LOW_TOM     7
#define DRUM_NONE        255

// Hit detection threshold
#define GYRO_HIT_THRESHOLD  -2500  // gyro_y threshold for hit detection

// Yaw offset for calibration
extern float yawOffset;

// Hit detection state
typedef struct {
    bool hitDetected;
    bool printedForGyro;  // Debounce flag
    uint8_t lastDrumSound;
} DrumHitState_t;

// Function prototypes
void DrumDetection_Init(void);
uint8_t DrumDetection_ProcessSensorData(sh2_SensorValue_t *sensorValue, DrumHitState_t *state);
void DrumDetection_QuaternionToEuler(float q_real, float q_i, float q_j, float q_k, 
                                     float *roll, float *pitch, float *yaw);
float DrumDetection_NormalizeYaw(float yaw);
void DrumDetection_SetYawOffset(float offset);

#endif // DRUM_DETECTION_H

