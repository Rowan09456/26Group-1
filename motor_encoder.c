
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/sync.h"

#include "motor_types.h"
#include "motor_encoder.h"

// Encoder timer interval
#define ENCODER_UPDATE_US 100

// Encoder internal variables
static struct repeating_timer encoder_timer;

static volatile bool encoder_enabled = false;
static volatile bool encoder_reverse = false;
static volatile float encoder_frequency = 0.0f;

static float phase_accumulator = 0.0f;
static uint8_t encoder_step = 0;

// Quadrature sequence: A leads B
static const uint8_t quadrature_sequence[4][2] = {
    {0, 0},
    {1, 0},
    {1, 1},
    {0, 1}
};

// =====================================================
// ENCODER INITIALIZATION
// =====================================================

void motor_encoder_init(void) {
    gpio_init(ENCODER_A_PIN);
    gpio_set_dir(ENCODER_A_PIN, GPIO_OUT);

    gpio_init(ENCODER_B_PIN);
    gpio_set_dir(ENCODER_B_PIN, GPIO_OUT);

    gpio_put(ENCODER_A_PIN, 0);
    gpio_put(ENCODER_B_PIN, 0);
}

// =====================================================
// ENCODER TIMER CALLBACK
// =====================================================

static bool encoder_timer_callback(
    struct repeating_timer *timer
) {
    (void)timer;

    // Disable encoder during IDLE or E-STOP
    if (!encoder_enabled || !gpio_get(12)) {
        gpio_put(ENCODER_A_PIN, 0);
        gpio_put(ENCODER_B_PIN, 0);

        phase_accumulator = 0.0f;
        encoder_step = 0;

        return true;
    }

    // Four quadrature transitions per pulse cycle
    float transition_frequency =
        encoder_frequency * 4.0f;

    float dt = ENCODER_UPDATE_US / 1000000.0f;

    phase_accumulator += transition_frequency * dt;

    while (phase_accumulator >= 1.0f) {

        phase_accumulator -= 1.0f;

        if (!encoder_reverse) {
            encoder_step = (encoder_step + 1) % 4;
        } else {
            encoder_step = (encoder_step + 3) % 4;
        }

        gpio_put(
            ENCODER_A_PIN,
            quadrature_sequence[encoder_step][0]
        );

        gpio_put(
            ENCODER_B_PIN,
            quadrature_sequence[encoder_step][1]
        );
    }

    return true;
}

// =====================================================
// START ENCODER TIMER
// =====================================================

bool motor_encoder_start(void) {
    return add_repeating_timer_us(
        -(int64_t)ENCODER_UPDATE_US,
        encoder_timer_callback,
        NULL,
        &encoder_timer
    );
}

// =====================================================
// UPDATE ENCODER USING MOTOR RPM
// =====================================================

void motor_encoder_update(void) {

    bool active =
        motor.state == MOTOR_STARTUP ||
        motor.state == MOTOR_RUNNING ||
        motor.state == MOTOR_SHUTDOWN;

    uint32_t flags = save_and_disable_interrupts();

    encoder_enabled = active;
    encoder_reverse = motor.reverse_direction;

    encoder_frequency =
        (motor.rpm * ENCODER_PPR) / 60.0f;

    if (!active) {
        encoder_frequency = 0.0f;

        gpio_put(ENCODER_A_PIN, 0);
        gpio_put(ENCODER_B_PIN, 0);

        phase_accumulator = 0.0f;
        encoder_step = 0;
    }

    restore_interrupts(flags);
}

// =====================================================
// STOP ENCODER
// =====================================================

void motor_encoder_stop(void) {

    uint32_t flags = save_and_disable_interrupts();

    encoder_enabled = false;
    encoder_frequency = 0.0f;

    phase_accumulator = 0.0f;
    encoder_step = 0;

    gpio_put(ENCODER_A_PIN, 0);
    gpio_put(ENCODER_B_PIN, 0);

    restore_interrupts(flags);
}
