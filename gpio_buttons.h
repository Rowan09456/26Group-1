
#ifndef GPIO_BUTTONS_H
#define GPIO_BUTTONS_H

#include <stdbool.h>
#include <stdint.h>

// Operator control pins
#define START_PIN       10
#define STOP_PIN        11
#define ESTOP_PIN       12
#define DIRECTION_PIN   13
#define RESET_PIN       18

// Initialize all buttons and switches
void gpio_buttons_init(void);

// Check for button press events
bool start_button_pressed(uint32_t now_ms);
bool stop_button_pressed(uint32_t now_ms);
bool reset_button_pressed(uint32_t now_ms);

// Read emergency stop and direction
bool estop_is_pressed(void);
bool direction_is_reverse(void);

#endif
