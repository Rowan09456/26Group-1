
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include "motor_types.h"
#include "motor_control.h"
#include "motor_oled.h"

// =====================================================
// OLED CONFIGURATION
// =====================================================

#define I2C_PORT    i2c0
#define I2C_SDA     4
#define I2C_SCL     5

#define OLED_ADDR   0x3C
#define OLED_WIDTH  128
#define OLED_PAGES  8

static uint8_t oled_buffer[OLED_WIDTH * OLED_PAGES];

// =====================================================
// OLED COMMANDS
// =====================================================

static void oled_command(uint8_t cmd) {
    uint8_t data[2] = {0x00, cmd};

    i2c_write_blocking(
        I2C_PORT,
        OLED_ADDR,
        data,
        2,
        false
    );
}

// =====================================================
// OLED INITIALIZATION
// =====================================================

void motor_oled_init(void) {
    i2c_init(I2C_PORT, 100000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);

    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

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

    oled_clear_buffer();
    oled_update();
}

// =====================================================
// OLED BUFFER
// =====================================================

void oled_clear_buffer(void) {
    memset(oled_buffer, 0, sizeof(oled_buffer));
}

void oled_update(void) {
    uint8_t packet[17];
    packet[0] = 0x40;

    for (int page = 0; page < OLED_PAGES; page++) {
        oled_command(0xB0 + page);
        oled_command(0x00);
        oled_command(0x10);

        for (int block = 0; block < 8; block++) {
            int offset =
                page * OLED_WIDTH + block * 16;

            memcpy(
                &packet[1],
                &oled_buffer[offset],
                16
            );

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
// OLED FONT - CORRECTED
// =====================================================

static const uint8_t font_letters[26][5] = {
    {0x7E,0x11,0x11,0x11,0x7E}, // A
    {0x7F,0x49,0x49,0x49,0x36}, // B
    {0x3E,0x41,0x41,0x41,0x22}, // C
    {0x7F,0x41,0x41,0x22,0x1C}, // D
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

// =====================================================
// CHARACTER DRAWING
// =====================================================

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

void oled_print(const char *text, int x, int page) {
    while (*text && x <= 122) {
        oled_draw_char(*text, x, page);
        x += 6;
        text++;
    }
}

// =====================================================
// MOTOR STATUS DISPLAY
// =====================================================

void oled_show_motor(void) {
    char line[24];

    oled_clear_buffer();

    oled_print("GROUP 1 MOTOR", 0, 0);
    oled_print("--------------------", 0, 1);

    snprintf(
        line,
        sizeof(line),
        "STATE:%s",
        state_name(motor.state)
    );
    oled_print(line, 0, 2);

    snprintf(
        line,
        sizeof(line),
        "FREQ:%.2f HZ",
        motor.frequency_hz
    );
    oled_print(line, 0, 3);

    snprintf(
        line,
        sizeof(line),
        "RPM:%.1f",
        motor.rpm
    );
    oled_print(line, 0, 4);

    snprintf(
        line,
        sizeof(line),
        "DIR:%s",
        motor.reverse_direction
            ? "REV ACB"
            : "FWD ABC"
    );
    oled_print(line, 0, 5);

    snprintf(
        line,
        sizeof(line),
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
