
#include <stdint.h>
#include <math.h>

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"
#include "hardware/sync.h"
#include "hardware/clocks.h"

#include "motor_types.h"
#include "motor_pwm.h"

// Three-phase PWM pins
#define PHASE_A_PIN 14
#define PHASE_B_PIN 15
#define PHASE_C_PIN 16

// Emergency stop input
#define ESTOP_PIN 12

// Waveform settings
#define PWM_CARRIER_HZ 20000
#define WAVE_UPDATE_US 200

#define PI_F 3.14159265358979323846f
#define TWO_PI_F (2.0f * PI_F)
#define PHASE_SHIFT (TWO_PI_F / 3.0f)

// PWM hardware
static uint phase_a_slice;
static uint phase_b_slice;
static uint phase_c_slice;
static uint16_t pwm_top;

// Shared waveform values
static volatile float waveform_frequency_hz = 0.0f;
static volatile bool waveform_reverse = false;
static volatile bool waveform_enabled = false;

static volatile float phase_a_value = 0.0f;
static volatile float phase_b_value = 0.0f;
static volatile float phase_c_value = 0.0f;

static float electrical_angle = 0.0f;
static struct repeating_timer waveform_timer;

// Convert sine value (-1 to +1) to PWM duty cycle
static uint16_t sine_to_pwm(float value) {
    float duty = (value + 1.0f) * 0.5f;

    if (duty < 0.0f) duty = 0.0f;
    if (duty > 1.0f) duty = 1.0f;

    return (uint16_t)(duty * pwm_top);
}

// Turn all PWM outputs off
static void pwm_outputs_off(void) {
    pwm_set_gpio_level(PHASE_A_PIN, 0);
    pwm_set_gpio_level(PHASE_B_PIN, 0);
    pwm_set_gpio_level(PHASE_C_PIN, 0);
}

// Initialize PWM hardware
void pwm_init_three_phase(void) {
    gpio_set_function(PHASE_A_PIN, GPIO_FUNC_PWM);
    gpio_set_function(PHASE_B_PIN, GPIO_FUNC_PWM);
    gpio_set_function(PHASE_C_PIN, GPIO_FUNC_PWM);

    phase_a_slice = pwm_gpio_to_slice_num(PHASE_A_PIN);
    phase_b_slice = pwm_gpio_to_slice_num(PHASE_B_PIN);
    phase_c_slice = pwm_gpio_to_slice_num(PHASE_C_PIN);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 1.0f);

    uint32_t sys_hz = clock_get_hz(clk_sys);
    uint32_t top = sys_hz / PWM_CARRIER_HZ;

    if (top < 2) top = 2;
    if (top > 65536) top = 65536;

    pwm_top = (uint16_t)(top - 1);
    pwm_config_set_wrap(&config, pwm_top);

    pwm_init(phase_a_slice, &config, false);

    if (phase_b_slice != phase_a_slice) {
        pwm_init(phase_b_slice, &config, false);
    }

    if (phase_c_slice != phase_a_slice &&
        phase_c_slice != phase_b_slice) {
        pwm_init(phase_c_slice, &config, false);
    }

    pwm_outputs_off();

    pwm_set_enabled(phase_a_slice, true);

    if (phase_b_slice != phase_a_slice) {
        pwm_set_enabled(phase_b_slice, true);
    }

    if (phase_c_slice != phase_a_slice &&
        phase_c_slice != phase_b_slice) {
        pwm_set_enabled(phase_c_slice, true);
    }
}

// Emergency shutdown
void waveform_emergency_off(void) {
    uint32_t flags = save_and_disable_interrupts();

    waveform_enabled = false;
    pwm_outputs_off();

    phase_a_value = 0.0f;
    phase_b_value = 0.0f;
    phase_c_value = 0.0f;

    restore_interrupts(flags);
}

// Waveform timer callback
static bool waveform_timer_callback(
    struct repeating_timer *timer
) {
    (void)timer;

    if (!waveform_enabled || !gpio_get(ESTOP_PIN)) {
        electrical_angle = 0.0f;
        phase_a_value = 0.0f;
        phase_b_value = 0.0f;
        phase_c_value = 0.0f;

        pwm_outputs_off();
        return true;
    }

    float frequency = waveform_frequency_hz;
    const float dt = WAVE_UPDATE_US / 1000000.0f;

    electrical_angle += TWO_PI_F * frequency * dt;

    if (electrical_angle >= TWO_PI_F) {
        electrical_angle -= TWO_PI_F;
    }

    float a = sinf(electrical_angle);
    float b;
    float c;

    if (!waveform_reverse) {
        // Forward: ABC
        b = sinf(electrical_angle - PHASE_SHIFT);
        c = sinf(electrical_angle + PHASE_SHIFT);
    } else {
        // Reverse: ACB
        b = sinf(electrical_angle + PHASE_SHIFT);
        c = sinf(electrical_angle - PHASE_SHIFT);
    }

    phase_a_value = a;
    phase_b_value = b;
    phase_c_value = c;

    pwm_set_gpio_level(PHASE_A_PIN, sine_to_pwm(a));
    pwm_set_gpio_level(PHASE_B_PIN, sine_to_pwm(b));
    pwm_set_gpio_level(PHASE_C_PIN, sine_to_pwm(c));

    return true;
}

// Start waveform timer
bool waveform_timer_start(void) {
    return add_repeating_timer_us(
        -(int64_t)WAVE_UPDATE_US,
        waveform_timer_callback,
        NULL,
        &waveform_timer
    );
}

// Update waveform based on motor state
void sync_waveform_with_motor(void) {
    bool active =
        motor.state == MOTOR_STARTUP ||
        motor.state == MOTOR_RUNNING ||
        motor.state == MOTOR_SHUTDOWN;

    uint32_t flags = save_and_disable_interrupts();

    waveform_enabled = active;
    waveform_frequency_hz = motor.frequency_hz;
    waveform_reverse = motor.reverse_direction;

    if (!active) {
        pwm_outputs_off();

        phase_a_value = 0.0f;
        phase_b_value = 0.0f;
        phase_c_value = 0.0f;
    }

    restore_interrupts(flags);
}

// Read phase values safely
void waveform_get_phases(float *a, float *b, float *c) {
    uint32_t flags = save_and_disable_interrupts();

    if (a) *a = phase_a_value;
    if (b) *b = phase_b_value;
    if (c) *c = phase_c_value;

    restore_interrupts(flags);
}

bool waveform_is_enabled(void) {
    return waveform_enabled;
}
