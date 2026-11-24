
#ifndef MOTOR_ENCODER_H
#define MOTOR_ENCODER_H

#include <stdbool.h>

// Encoder output pins
#define ENCODER_A_PIN 20
#define ENCODER_B_PIN 21

// Simulated encoder resolution
#define ENCODER_PPR 20

// Initialize encoder GPIO
void motor_encoder_init(void);

// Start encoder timer
bool motor_encoder_start(void);

// Synchronize encoder with motor data
void motor_encoder_update(void);

// Disable encoder outputs
void motor_encoder_stop(void);

#endif
