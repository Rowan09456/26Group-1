
#ifndef MOTOR_PWM_H
#define MOTOR_PWM_H

#include <stdbool.h>

// Initialize three-phase PWM outputs
void pwm_init_three_phase(void);

// Start the repeating waveform timer
bool waveform_timer_start(void);

// Synchronize PWM with motor state
void sync_waveform_with_motor(void);

// Immediately disable PWM output
void waveform_emergency_off(void);

// Read current phase values
void waveform_get_phases(float *a, float *b, float *c);

// Check whether waveform generation is active
bool waveform_is_enabled(void);

#endif
