
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"

#include "motor_types.h"
#include "motor_control.h"
#include "motor_pwm.h"
#include "motor_oled.h"
#include "gpio_buttons.h"

// =====================================================
// GROUP 1 - MOTOR CONTROLLER
// Raspberry Pi Pico 2W
// Main Program
// =====================================================

#define UPDATE_MS   100
#define DISPLAY_MS  250
#define PRINT_MS    1000

// =====================================================
// TIME FUNCTION
// =====================================================

static uint32_t current_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

// =====================================================
// USB SERIAL STATUS
// =====================================================

static void print_status(void) {
    float a, b, c;

    waveform_get_phases(&a, &b, &c);

    printf("\n================================\n");
    printf("GROUP 1 MOTOR CONTROLLER\n");
    printf("================================\n");

    printf("STATE: %s\n", state_name(motor.state));
    printf("FREQUENCY: %.2f Hz\n", motor.frequency_hz);
    printf("RPM: %.1f\n", motor.rpm);

    printf(
        "DIRECTION: %s\n",
        motor.reverse_direction
            ? "REVERSE (ACB)"
            : "FORWARD (ABC)"
    );

    printf("SLIP FACTOR: %.2f\n", motor.slip_factor);

    printf("\nTHREE-PHASE PWM\n");
    printf("PHASE A: %+.3f\n", a);
    printf("PHASE B: %+.3f\n", b);
    printf("PHASE C: %+.3f\n", c);

    printf(
        "PWM OUTPUTS: %s\n",
        waveform_is_enabled() ? "ACTIVE" : "OFF"
    );

    printf("PWM PINS: GP14 / GP15 / GP16\n");
    printf("================================\n");
}

// =====================================================
// MAIN PROGRAM
// =====================================================

int main(void) {

    stdio_init_all();

    // Initialize operator inputs
    gpio_buttons_init();

    // Initialize motor data
    motor_control_init();

    // Initialize OLED
    motor_oled_init();

    // Initialize PWM
    pwm_init_three_phase();
    sync_waveform_with_motor();

    // Start waveform timer
    bool timer_ok = waveform_timer_start();

    if (!timer_ok) {
        printf("ERROR: Waveform timer failed\n");

        while (true) {
            waveform_emergency_off();
            sleep_ms(100);
        }
    }

    sleep_ms(2000);

    printf("\n================================\n");
    printf("GROUP 1 MOTOR CONTROLLER\n");
    printf("PICO 2W - THREE-PHASE PWM\n");
    printf("================================\n");

    printf("START: GP10\n");
    printf("STOP: GP11\n");
    printf("E-STOP: GP12\n");
    printf("DIRECTION: GP13\n");
    printf("RESET: GP18\n");

    printf("PHASE A: GP14\n");
    printf("PHASE B: GP15\n");
    printf("PHASE C: GP16\n");

    printf("FREQUENCY: 50-60 HZ\n");
    printf("RAMP RATE: 0.1 HZ/SEC\n");
    printf("PWM UPDATE: 5000 HZ\n");

    printf("================================\n");

    oled_show_motor();
    print_status();

    uint32_t last_update_ms = current_ms();
    uint32_t last_display_ms = last_update_ms;
    uint32_t last_print_ms = last_update_ms;

    // =================================================
    // MAIN LOOP
    // =================================================

    while (true) {

        uint32_t now_ms = current_ms();

        // Emergency Stop
        if (estop_is_pressed() &&
            motor.state != MOTOR_ESTOP) {
            emergency_stop();
        }

        // Read buttons
        bool start_event =
            start_button_pressed(now_ms);

        bool stop_event =
            stop_button_pressed(now_ms);

        bool reset_event =
            reset_button_pressed(now_ms);

        // Reset
        if (reset_event) {
            reset_motor();
        }

        // Direction switch
        if (motor.state == MOTOR_IDLE) {

            bool reverse = direction_is_reverse();

            if (reverse != motor.reverse_direction) {

                motor.reverse_direction = reverse;
                sync_waveform_with_motor();

                printf(
                    "DIRECTION CHANGED: %s\n",
                    reverse
                        ? "REVERSE (ACB)"
                        : "FORWARD (ABC)"
                );
            }
        }

        // Start / Stop
        if (motor.state != MOTOR_ESTOP &&
            !estop_is_pressed()) {

            if (start_event) {
                start_motor();
            }
            else if (stop_event) {
                normal_stop();
            }
        }

        // Frequency ramp
        if ((uint32_t)(now_ms - last_update_ms)
            >= UPDATE_MS) {

            float dt =
                (uint32_t)(now_ms - last_update_ms)
                / 1000.0f;

            update_motor(dt);
            last_update_ms = now_ms;
        }

        // OLED update
        if ((uint32_t)(now_ms - last_display_ms)
            >= DISPLAY_MS) {

            oled_show_motor();
            last_display_ms = now_ms;
        }

        // USB serial status
        if ((uint32_t)(now_ms - last_print_ms)
            >= PRINT_MS) {

            print_status();
            last_print_ms = now_ms;
        }

        sleep_ms(5);
    }

    return 0;
}
