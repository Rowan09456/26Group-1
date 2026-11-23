
#include <stdio.h>

#include "pico/stdlib.h"

#include "motor_control.h"
#include "motor_pwm.h"

#define MIN_FREQ_HZ 50.0f
#define MAX_FREQ_HZ 60.0f
#define RAMP_RATE   0.1f
#define SLIP_FACTOR 0.95f
#define MOTOR_POLES 4

#define ESTOP_PIN     12
#define DIRECTION_PIN 13

motor_data_t motor = {
    .state = MOTOR_IDLE,
    .frequency_hz = 0.0f,
    .slip_factor = SLIP_FACTOR,
    .rpm = 0.0f,
    .reverse_direction = false,
    .emergency_stop = false
};

void motor_control_init(void) {
    motor.reverse_direction = !gpio_get(DIRECTION_PIN);
    sync_waveform_with_motor();
}

const char *state_name(motor_state_t state) {
    switch (state) {
        case MOTOR_IDLE:     return "IDLE";
        case MOTOR_STARTUP:  return "STARTUP";
        case MOTOR_RUNNING:  return "RUNNING";
        case MOTOR_SHUTDOWN: return "SHUTDOWN";
        case MOTOR_ESTOP:    return "E-STOP";
        default:             return "UNKNOWN";
    }
}

void update_rpm(void) {
    if (motor.state == MOTOR_IDLE ||
        motor.state == MOTOR_ESTOP) {
        motor.rpm = 0.0f;
    } else {
        motor.rpm =
            (120.0f * motor.frequency_hz / MOTOR_POLES)
            * motor.slip_factor;
    }
}

void start_motor(void) {
    if (motor.state != MOTOR_IDLE) {
        printf("ERROR: Start only allowed in IDLE\n");
        return;
    }

    motor.state = MOTOR_STARTUP;
    motor.frequency_hz = MIN_FREQ_HZ;
    motor.emergency_stop = false;

    update_rpm();
    sync_waveform_with_motor();

    printf("\nSTART BUTTON PRESSED\n");
    printf("Motor starting at 50 Hz\n");
}

void normal_stop(void) {
    if (motor.state == MOTOR_IDLE) {
        printf("ERROR: Motor not running\n");
        return;
    }

    if (motor.state == MOTOR_ESTOP) {
        printf("ERROR: Emergency stop active\n");
        return;
    }

    if (motor.state == MOTOR_SHUTDOWN) {
        printf("Shutdown already in progress\n");
        return;
    }

    motor.state = MOTOR_SHUTDOWN;
    printf("\nNORMAL STOP PRESSED\n");
}

void emergency_stop(void) {
    waveform_emergency_off();

    motor.state = MOTOR_ESTOP;
    motor.frequency_hz = 0.0f;
    motor.rpm = 0.0f;
    motor.emergency_stop = true;

    sync_waveform_with_motor();

    printf("\n!!! EMERGENCY STOP !!!\n");
}

void reset_motor(void) {
    if (motor.state != MOTOR_ESTOP) {
        printf("RESET ignored: Not in E-STOP\n");
        return;
    }

    if (!gpio_get(ESTOP_PIN)) {
        printf("ERROR: Release E-STOP first\n");
        return;
    }

    motor.state = MOTOR_IDLE;
    motor.frequency_hz = 0.0f;
    motor.rpm = 0.0f;
    motor.emergency_stop = false;
    motor.reverse_direction = !gpio_get(DIRECTION_PIN);

    sync_waveform_with_motor();

    printf("\nRESET BUTTON PRESSED\n");
    printf("Motor reset to IDLE\n");
}

void update_motor(float dt) {
    if (motor.state == MOTOR_STARTUP) {
        motor.frequency_hz += RAMP_RATE * dt;

        if (motor.frequency_hz >= MAX_FREQ_HZ) {
            motor.frequency_hz = MAX_FREQ_HZ;
            motor.state = MOTOR_RUNNING;

            printf("Motor reached 60 Hz\n");
        }
    }
    else if (motor.state == MOTOR_SHUTDOWN) {
        motor.frequency_hz -= RAMP_RATE * dt;

        if (motor.frequency_hz <= MIN_FREQ_HZ) {
            motor.frequency_hz = 0.0f;
            motor.state = MOTOR_IDLE;

            printf("Normal shutdown complete\n");
        }
    }

    update_rpm();
    sync_waveform_with_motor();
}
