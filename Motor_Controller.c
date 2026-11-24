#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"
#include "hardware/sync.h"
#include "hardware/clocks.h"

// =====================================================
// GROUP 1 - MOTOR CONTROLLER
// Raspberry Pi Pico 2W - Pico C SDK
// =====================================================

// OPERATOR CONTROL PANEL
#define START_PIN       10
#define STOP_PIN        11
#define ESTOP_PIN       12
#define DIRECTION_PIN   13
#define RESET_PIN       18

// THREE-PHASE PWM OUTPUTS
#define PHASE_A_PIN     14
#define PHASE_B_PIN     15
#define PHASE_C_PIN     16

// OLED DISPLAY
#define I2C_PORT        i2c0
#define I2C_SDA         4
#define I2C_SCL         5
#define OLED_ADDR       0x3C
#define OLED_WIDTH      128
#define OLED_PAGES      8

// MOTOR REQUIREMENTS
#define MIN_FREQ_HZ     50.0f
#define MAX_FREQ_HZ     60.0f
#define RAMP_RATE       0.1f
#define SLIP_FACTOR     0.95f
#define MOTOR_POLES     4

// TIMING
#define UPDATE_MS       100
#define DISPLAY_MS      250
#define PRINT_MS        1000
#define DEBOUNCE_MS     30
#define WAVE_UPDATE_US  200

// PWM
#define PWM_WRAP        6249
#define PWM_CARRIER_HZ  20000

#define PI_F            3.14159265358979323846f
#define TWO_PI_F        (2.0f * PI_F)
#define PHASE_SHIFT     (TWO_PI_F / 3.0f)

// =====================================================
// MOTOR STATE MACHINE
// =====================================================

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

static motor_data_t motor = {
    .state = MOTOR_IDLE,
    .frequency_hz = 0.0f,
    .slip_factor = SLIP_FACTOR,
    .rpm = 0.0f,
    .reverse_direction = false,
    .emergency_stop = false
};

// =====================================================
// SHARED WAVEFORM DATA
// =====================================================

static volatile float waveform_frequency_hz = 0.0f;
static volatile bool waveform_reverse = false;
static volatile bool waveform_enabled = false;

static volatile float phase_a_value = 0.0f;
static volatile float phase_b_value = 0.0f;
static volatile float phase_c_value = 0.0f;

static float electrical_angle = 0.0f;
static struct repeating_timer waveform_timer;

// =====================================================
// BUTTON DEBOUNCING
// =====================================================

typedef struct {
    uint pin;
    bool stable_pressed;
    bool last_raw_pressed;
    uint32_t last_change_ms;
} button_t;

static button_t start_btn = {.pin = START_PIN};
static button_t stop_btn  = {.pin = STOP_PIN};
static button_t reset_btn = {.pin = RESET_PIN};

static uint32_t current_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

static void button_init(button_t *button) {
    gpio_init(button->pin);
    gpio_set_dir(button->pin, GPIO_IN);
    gpio_pull_up(button->pin);

    bool pressed = !gpio_get(button->pin);

    button->stable_pressed = pressed;
    button->last_raw_pressed = pressed;
    button->last_change_ms = current_ms();
}

static bool button_press_event(button_t *button, uint32_t now_ms) {
    bool raw = !gpio_get(button->pin);

    if (raw != button->last_raw_pressed) {
        button->last_raw_pressed = raw;
        button->last_change_ms = now_ms;
    }

    if ((uint32_t)(now_ms - button->last_change_ms) >= DEBOUNCE_MS &&
        raw != button->stable_pressed) {

        button->stable_pressed = raw;
        return raw;
    }

    return false;
}

// =====================================================
// OLED DRIVER
// =====================================================

static uint8_t oled_buffer[OLED_WIDTH * OLED_PAGES];

static void oled_command(uint8_t cmd) {
    uint8_t data[2] = {0x00, cmd};
    i2c_write_blocking(I2C_PORT, OLED_ADDR, data, 2, false);
}

static void oled_init(void) {
    sleep_ms(100);

    oled_command(0xAE);
    oled_command(0xD5);
    oled_command(0x80);
    oled_command(0xA8);
    oled_command(0x3F);
    oled_command(0xD3);
    oled_command(0x00);
    oled_command(0x40);
    oled_command(0x8D);
    oled_command(0x14);
    oled_command(0x20);
    oled_command(0x02);
    oled_command(0xA1);
    oled_command(0xC8);
    oled_command(0xDA);
    oled_command(0x12);
    oled_command(0x81);
    oled_command(0x7F);
    oled_command(0xD9);
    oled_command(0xF1);
    oled_command(0xDB);
    oled_command(0x40);
    oled_command(0xA4);
    oled_command(0xA6);
    oled_command(0xAF);
}

static void oled_clear_buffer(void) {
    memset(oled_buffer, 0, sizeof(oled_buffer));
}

static void oled_update(void) {
    uint8_t packet[17];
    packet[0] = 0x40;

    for (int page = 0; page < OLED_PAGES; page++) {
        oled_command(0xB0 + page);
        oled_command(0x00);
        oled_command(0x10);

        for (int block = 0; block < 8; block++) {
            int offset = page * OLED_WIDTH + block * 16;

            memcpy(&packet[1], &oled_buffer[offset], 16);

            i2c_write_blocking(
                I2C_PORT,
                OLED_ADDR,
                packet,
                sizeof(packet),
                false
            );
        }
    }
}

// =====================================================
// OLED FONT - CORRECTED ALPHABET
// =====================================================

static const uint8_t font_letters[26][5] = {
    {0x7E,0x11,0x11,0x11,0x7E}, // A
    {0x7F,0x49,0x49,0x49,0x36}, // B
    {0x3E,0x41,0x41,0x41,0x22}, // C
    {0x7F,0x41,0x41,0x22,0x1C}, // D - FIXED
    {0x7F,0x49,0x49,0x49,0x41}, // E
    {0x7F,0x09,0x09,0x09,0x01}, // F
    {0x3E,0x41,0x49,0x49,0x7A}, // G
    {0x7F,0x08,0x08,0x08,0x7F}, // H
    {0x41,0x41,0x7F,0x41,0x41}, // I
    {0x20,0x40,0x41,0x3F,0x01}, // J
    {0x7F,0x08,0x14,0x22,0x41}, // K
    {0x7F,0x40,0x40,0x40,0x40}, // L
    {0x7F,0x02,0x0C,0x02,0x7F}, // M
    {0x7F,0x04,0x08,0x10,0x7F}, // N
    {0x3E,0x41,0x41,0x41,0x3E}, // O
    {0x7F,0x09,0x09,0x09,0x06}, // P
    {0x3E,0x41,0x51,0x21,0x5E}, // Q
    {0x7F,0x09,0x19,0x29,0x46}, // R
    {0x46,0x49,0x49,0x49,0x31}, // S
    {0x01,0x01,0x7F,0x01,0x01}, // T
    {0x3F,0x40,0x40,0x40,0x3F}, // U
    {0x1F,0x20,0x40,0x20,0x1F}, // V
    {0x7F,0x20,0x18,0x20,0x7F}, // W
    {0x63,0x14,0x08,0x14,0x63}, // X
    {0x03,0x04,0x78,0x04,0x03}, // Y
    {0x61,0x51,0x49,0x45,0x43}  // Z
};

static const uint8_t font_digits[10][5] = {
    {0x3E,0x51,0x49,0x45,0x3E},
    {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30},
    {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E}
};

static void oled_draw_char(char c, int x, int page) {
    if (x < 0 || x > 122 ||
        page < 0 || page >= OLED_PAGES) {
        return;
    }

    uint8_t glyph[5] = {0};

    if (c >= 'a' && c <= 'z') {
        c -= 32;
    }

    if (c >= 'A' && c <= 'Z') {
        memcpy(glyph, font_letters[c - 'A'], 5);
    }
    else if (c >= '0' && c <= '9') {
        memcpy(glyph, font_digits[c - '0'], 5);
    }
    else if (c == ':') {
        glyph[2] = 0x36;
    }
    else if (c == '.') {
        glyph[2] = 0x60;
    }
    else if (c == '-') {
        glyph[1] = 0x08;
        glyph[2] = 0x08;
        glyph[3] = 0x08;
    }
    else if (c == '!') {
        glyph[2] = 0x5F;
    }
    else if (c == '/') {
        glyph[0] = 0x20;
        glyph[1] = 0x10;
        glyph[2] = 0x08;
        glyph[3] = 0x04;
        glyph[4] = 0x02;
    }

    int index = page * OLED_WIDTH + x;

    for (int i = 0; i < 5; i++) {
        oled_buffer[index + i] = glyph[i];
    }

    oled_buffer[index + 5] = 0;
}

static void oled_print(const char *text, int x, int page) {
    while (*text && x <= 122) {
        oled_draw_char(*text, x, page);
        x += 6;
        text++;
    }
}

// =====================================================
// THREE-PHASE PWM GENERATOR
// =====================================================

static uint phase_a_slice;
static uint phase_b_slice;
static uint phase_c_slice;

static uint16_t sine_to_pwm(float value) {
    float duty = (value + 1.0f) * 0.5f;

    if (duty < 0.0f) duty = 0.0f;
    if (duty > 1.0f) duty = 1.0f;

    return (uint16_t)(duty * (PWM_WRAP + 1));
}

static void pwm_outputs_off(void) {
    pwm_set_gpio_level(PHASE_A_PIN, 0);
    pwm_set_gpio_level(PHASE_B_PIN, 0);
    pwm_set_gpio_level(PHASE_C_PIN, 0);
}

static void pwm_init_three_phase(void) {
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

    pwm_config_set_wrap(&config, top - 1);

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

// =====================================================
// WAVEFORM TIMER
// =====================================================

static bool waveform_timer_callback(struct repeating_timer *timer) {
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
        // FORWARD ABC
        b = sinf(electrical_angle - PHASE_SHIFT);
        c = sinf(electrical_angle + PHASE_SHIFT);
    }
    else {
        // REVERSE ACB
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

static void sync_waveform_with_motor(void) {
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

// =====================================================
// MOTOR CONTROL FUNCTIONS
// =====================================================

static const char* state_name(motor_state_t state) {
    switch (state) {
        case MOTOR_IDLE:     return "IDLE";
        case MOTOR_STARTUP:  return "STARTUP";
        case MOTOR_RUNNING:  return "RUNNING";
        case MOTOR_SHUTDOWN: return "SHUTDOWN";
        case MOTOR_ESTOP:    return "E-STOP";
        default:             return "UNKNOWN";
    }
}

static void update_rpm(void) {
    if (motor.state == MOTOR_IDLE ||
        motor.state == MOTOR_ESTOP) {
        motor.rpm = 0.0f;
    }
    else {
        motor.rpm =
            (120.0f * motor.frequency_hz /
             MOTOR_POLES) * motor.slip_factor;
    }
}

static void start_motor(void) {
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

static void normal_stop(void) {
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

static void emergency_stop(void) {
    waveform_enabled = false;
    pwm_outputs_off();

    motor.state = MOTOR_ESTOP;
    motor.frequency_hz = 0.0f;
    motor.rpm = 0.0f;
    motor.emergency_stop = true;

    sync_waveform_with_motor();

    printf("\n!!! EMERGENCY STOP !!!\n");
}

static void reset_motor(void) {
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

static void update_motor(float dt) {
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

// =====================================================
// OLED MOTOR STATUS
// =====================================================

static void oled_show_motor(void) {
    char line[24];

    oled_clear_buffer();

    oled_print("GROUP 1 MOTOR", 0, 0);
    oled_print("--------------------", 0, 1);

    snprintf(
        line, sizeof(line),
        "STATE:%s",
        state_name(motor.state)
    );
    oled_print(line, 0, 2);

    snprintf(
        line, sizeof(line),
        "FREQ:%.2f HZ",
        motor.frequency_hz
    );
    oled_print(line, 0, 3);

    snprintf(
        line, sizeof(line),
        "RPM:%.1f",
        motor.rpm
    );
    oled_print(line, 0, 4);

    snprintf(
        line, sizeof(line),
        "DIR:%s",
        motor.reverse_direction
        ? "REV ACB" : "FWD ABC"
    );
    oled_print(line, 0, 5);

    snprintf(
        line, sizeof(line),
        "SLIP:%.2f",
        motor.slip_factor
    );
    oled_print(line, 0, 6);

    if (motor.state == MOTOR_ESTOP) {
        oled_print("! EMERGENCY STOP !", 0, 7);
    }
    else if (motor.state == MOTOR_IDLE) {
        oled_print("READY TO START", 0, 7);
    }
    else if (motor.state == MOTOR_RUNNING) {
        oled_print("MOTOR RUNNING", 0, 7);
    }
    else if (motor.state == MOTOR_STARTUP) {
        oled_print("RAMPING UP", 0, 7);
    }
    else {
        oled_print("RAMPING DOWN", 0, 7);
    }

    oled_update();
}

// =====================================================
// USB SERIAL STATUS
// =====================================================

static void print_status(void) {
    float a, b, c;

    uint32_t flags = save_and_disable_interrupts();

    a = phase_a_value;
    b = phase_b_value;
    c = phase_c_value;

    restore_interrupts(flags);

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
        waveform_enabled ? "ACTIVE" : "OFF"
    );

    printf("PWM PINS: GP14 / GP15 / GP16\n");
    printf("================================\n");
}

// =====================================================
// MAIN PROGRAM
// =====================================================

int main(void) {
    stdio_init_all();

    // Buttons
    button_init(&start_btn);
    button_init(&stop_btn);
    button_init(&reset_btn);

    // Emergency Stop
    gpio_init(ESTOP_PIN);
    gpio_set_dir(ESTOP_PIN, GPIO_IN);
    gpio_pull_up(ESTOP_PIN);

    // Direction switch
    gpio_init(DIRECTION_PIN);
    gpio_set_dir(DIRECTION_PIN, GPIO_IN);
    gpio_disable_pulls(DIRECTION_PIN);

    motor.reverse_direction = !gpio_get(DIRECTION_PIN);

    // OLED I2C
    i2c_init(I2C_PORT, 100000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);

    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    oled_init();

    // OLED STARTUP TEST
    oled_show_motor();

    // PWM
    pwm_init_three_phase();
    sync_waveform_with_motor();

    bool timer_ok = add_repeating_timer_us(
        -(int64_t)WAVE_UPDATE_US,
        waveform_timer_callback,
        NULL,
        &waveform_timer
    );

    if (!timer_ok) {
        printf("ERROR: Waveform timer failed\n");

        while (true) {
            pwm_outputs_off();
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

    while (true) {
        uint32_t now_ms = current_ms();

        // Emergency Stop
        if (!gpio_get(ESTOP_PIN) &&
            motor.state != MOTOR_ESTOP) {
            emergency_stop();
        }

        // Read buttons
        bool start_event =
            button_press_event(&start_btn, now_ms);

        bool stop_event =
            button_press_event(&stop_btn, now_ms);

        bool reset_event =
            button_press_event(&reset_btn, now_ms);

        // Reset
        if (reset_event) {
            reset_motor();
        }

        // Direction
        if (motor.state == MOTOR_IDLE) {
            bool reverse = !gpio_get(DIRECTION_PIN);

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
            gpio_get(ESTOP_PIN)) {

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

        // Serial output
        if ((uint32_t)(now_ms - last_print_ms)
                >= PRINT_MS) {

            print_status();
            last_print_ms = now_ms;
        }

        sleep_ms(5);
    }

    return 0;
}
