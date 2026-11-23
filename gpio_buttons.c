
#include "pico/stdlib.h"
#include "gpio_buttons.h"

#define DEBOUNCE_MS 30

typedef struct {
    uint pin;
    bool stable_pressed;
    bool last_raw_pressed;
    uint32_t last_change_ms;
} button_t;

static button_t start_btn = {.pin = START_PIN};
static button_t stop_btn  = {.pin = STOP_PIN};
static button_t reset_btn = {.pin = RESET_PIN};

// Initialize one pushbutton
static void button_init(button_t *button) {
    gpio_init(button->pin);
    gpio_set_dir(button->pin, GPIO_IN);
    gpio_pull_up(button->pin);

    bool pressed = !gpio_get(button->pin);

    button->stable_pressed = pressed;
    button->last_raw_pressed = pressed;
    button->last_change_ms = to_ms_since_boot(get_absolute_time());
}

// Detect a debounced press event
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

// Initialize all operator inputs
void gpio_buttons_init(void) {
    button_init(&start_btn);
    button_init(&stop_btn);
    button_init(&reset_btn);

    // E-STOP: active LOW
    gpio_init(ESTOP_PIN);
    gpio_set_dir(ESTOP_PIN, GPIO_IN);
    gpio_pull_up(ESTOP_PIN);

    // Direction switch
    gpio_init(DIRECTION_PIN);
    gpio_set_dir(DIRECTION_PIN, GPIO_IN);

    // Preserve original external pull resistor setup
    gpio_disable_pulls(DIRECTION_PIN);
}

bool start_button_pressed(uint32_t now_ms) {
    return button_press_event(&start_btn, now_ms);
}

bool stop_button_pressed(uint32_t now_ms) {
    return button_press_event(&stop_btn, now_ms);
}

bool reset_button_pressed(uint32_t now_ms) {
    return button_press_event(&reset_btn, now_ms);
}

bool estop_is_pressed(void) {
    return !gpio_get(ESTOP_PIN);
}

bool direction_is_reverse(void) {
    return !gpio_get(DIRECTION_PIN);
}
