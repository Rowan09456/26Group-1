
#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include "motor_types.h"

void motor_control_init(void);

const char *state_name(motor_state_t state);

void update_rpm(void);

void start_motor(void);

void normal_stop(void);

void emergency_stop(void);

void reset_motor(void);

void update_motor(float dt);

#endif
