
#ifndef MOTOR_TYPES_H
#define MOTOR_TYPES_H

#include <stdbool.h>

typedef enum {
    MOTOR_IDLE,
    MOTOR_STARTUP,
    MOTOR_RUNNING,
    MOTOR_SHUTDOWN,
    MOTOR_ESTOP
} motor_state_t;

typedef struct {
    motor_state_t state;
    float frequency_hz;
    float slip_factor;
    float rpm;
    bool reverse_direction;
    bool emergency_stop;
} motor_data_t;

extern motor_data_t motor;

#endif
