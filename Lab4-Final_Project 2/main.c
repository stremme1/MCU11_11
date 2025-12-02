// main.c
// DAC-based Drum Sample Player for STM32L432KC
//
// Author: Emmett Stralka
// Email: estralka@hmc.edu
// Date: 9/29/25
//
// Description: Plays WAV drum samples from memory

#include "STM32L432KC_RCC.h"
#include "STM32L432KC_GPIO.h"
#include "STM32L432KC_FLASH.h"
#include "STM32L432KC_DAC.h"
#include "STM32L432KC_TIMER.h"  // For ms_delay

// Include drum sample arrays header (the .c files are compiled separately)
#include "wav_arrays/drum_samples.h"

// Drum sample rate (all samples converted to 22.05 kHz)
#define DRUM_SAMPLE_RATE 22050

// Function to play a drum sample
void play_drum_sample(const int16_t* data, uint32_t length, uint32_t sample_rate) {
    DAC_PlayWAV(data, length, sample_rate);
}

// Main function
int main(void) {
    // Initialize system
    configureFlash();
    configureClock();
    
    // Initialize DAC for audio output (using channel 1 on PA4)
    DAC_InitAudio(DAC_CHANNEL_1);
    
    // CRITICAL TEST: Direct register write test
    // This bypasses all functions to test if the DAC hardware works at all
    // Measure PA4 with multimeter - you should see voltage changes
    
    // Test 1: Maximum value (4095) - should be ~3.1V
    DAC->DHR12R1 = 4095;
    ms_delay(2000);  // Hold for 2 seconds
    
    // Test 2: Mid-point (2048) - should be ~1.65V  
    DAC->DHR12R1 = 2048;
    ms_delay(2000);  // Hold for 2 seconds
    
    // Test 3: Quarter (1024) - should be ~0.825V
    DAC->DHR12R1 = 1024;
    ms_delay(2000);  // Hold for 2 seconds
    
    // Test 4: Minimum (0) - should be ~0.2V (buffer minimum)
    DAC->DHR12R1 = 0;
    ms_delay(2000);  // Hold for 2 seconds
    
    // Test 5: Back to maximum
    DAC->DHR12R1 = 4095;
    ms_delay(2000);  // Hold for 2 seconds
    
    // Play drum samples in sequence for testing
    // This plays all 8 drum sounds in order
    while(1) {
        // Kick
        play_drum_sample(kick_sample_data, kick_sample_length, kick_sample_sample_rate);
        ms_delay(200);
        
        // Snare
        play_drum_sample(snare_sample_data, snare_sample_length, snare_sample_sample_rate);
        ms_delay(200);
        
        // Hi-Hat Closed
        play_drum_sample(hihat_closed_sample_data, hihat_closed_sample_length, hihat_closed_sample_sample_rate);
        ms_delay(200);
        
        // Hi-Hat Open
        play_drum_sample(hihat_open_sample_data, hihat_open_sample_length, hihat_open_sample_sample_rate);
        ms_delay(200);
        
        // Crash
        play_drum_sample(crash_sample_data, crash_sample_length, crash_sample_sample_rate);
        ms_delay(200);
        
        // Ride
        play_drum_sample(ride_sample_data, ride_sample_length, ride_sample_sample_rate);
        ms_delay(200);
        
        // Tom High
        play_drum_sample(tom_high_sample_data, tom_high_sample_length, tom_high_sample_sample_rate);
        ms_delay(200);
        
        // Tom Low
        play_drum_sample(tom_low_sample_data, tom_low_sample_length, tom_low_sample_sample_rate);
        ms_delay(500);  // Longer pause before repeating
    }
    
    return 0;
}
