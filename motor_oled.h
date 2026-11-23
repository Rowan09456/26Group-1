
#ifndef MOTOR_OLED_H
#define MOTOR_OLED_H

// Initialize I2C and OLED display
void motor_oled_init(void);

// Clear the display buffer
void oled_clear_buffer(void);

// Print text at x position and page (0-7)
void oled_print(const char *text, int x, int page);

// Send display buffer to OLED
void oled_update(void);

// Display current motor status
void oled_show_motor(void);

#endif
