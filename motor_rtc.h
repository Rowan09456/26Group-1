
#ifndef MOTOR_RTC_H
#define MOTOR_RTC_H

#include <stdbool.h>
#include <stdint.h>

// DS3231 hardware configuration
#define RTC_SDA_PIN 26
#define RTC_SCL_PIN 27
#define RTC_ADDRESS 0x68

// Allow C and C++ compatibility
#ifdef __cplusplus
extern "C" {
#endif

bool motor_rtc_init(void);
void motor_rtc_print(void);

#ifdef __cplusplus
}
#endif

#endif
