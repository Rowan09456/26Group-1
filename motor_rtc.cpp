
#include <stdio.h>
#include <time.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "pico/aon_timer.h"

#include "motor_rtc.h"
#include "ds3231.h"

// RTC Configuration
#define RTC_I2C i2c1
#define RTC_BAUDRATE 100000
#define RTC_TIMEOUT_US 10000

static bool rtc_ready = false;

// ========================================
// Initialize RTC at power-up
// ========================================

bool motor_rtc_init(void)
{
    // Initialize I2C1
    i2c_init(RTC_I2C, RTC_BAUDRATE);

    gpio_set_function(
        RTC_SDA_PIN,
        GPIO_FUNC_I2C
    );

    gpio_set_function(
        RTC_SCL_PIN,
        GPIO_FUNC_I2C
    );

    gpio_pull_up(RTC_SDA_PIN);
    gpio_pull_up(RTC_SCL_PIN);

    sleep_ms(100);

    // Initialize professor's DS3231 driver
    ds3231::init(RTC_I2C, RTC_ADDRESS);

    printf("\n================================\n");
    printf("RTC INITIALIZATION\n");
    printf("================================\n");

    // Verify DS3231 communication
    uint8_t reg = 0x00;

    int result = i2c_write_timeout_us(
        RTC_I2C,
        RTC_ADDRESS,
        &reg,
        1,
        false,
        RTC_TIMEOUT_US
    );

    if (result != 1)
    {
        printf("[RTC] DS3231 not detected\n");
        return false;
    }

    printf("[RTC] DS3231 detected\n");

    // Read external RTC once at startup
    ds3231_dateTime dt = ds3231::read_time();

    // Validate date and time
    if (dt.sec > 59 ||
        dt.min > 59 ||
        dt.hour > 23 ||
        dt.date < 1 ||
        dt.date > 31 ||
        dt.month < 1 ||
        dt.month > 12 ||
        dt.year > 99 ||
        dt.day_of_week < 1 ||
        dt.day_of_week > 7)
    {
        printf("[RTC] Invalid date or time\n");
        return false;
    }

    // Convert DS3231 data to SDK calendar format
    struct tm sdk_time = {};

    sdk_time.tm_sec = dt.sec;
    sdk_time.tm_min = dt.min;
    sdk_time.tm_hour = dt.hour;
    sdk_time.tm_mday = dt.date;
    sdk_time.tm_mon = dt.month - 1;
    sdk_time.tm_year = dt.year + 100;
    sdk_time.tm_wday = dt.day_of_week - 1;
    sdk_time.tm_isdst = 0;

    printf(
        "[RTC] Power-up time: "
        "%04d-%02d-%02d %02d:%02d:%02d\n",
        sdk_time.tm_year + 1900,
        sdk_time.tm_mon + 1,
        sdk_time.tm_mday,
        sdk_time.tm_hour,
        sdk_time.tm_min,
        sdk_time.tm_sec
    );

    // Initialize Pico SDK internal clock
    if (!aon_timer_start_calendar(&sdk_time))
    {
        printf("[RTC] SDK clock initialization failed\n");
        return false;
    }

    rtc_ready = true;

    printf("[RTC] SDK clock initialized\n");
    printf("[RTC] SDK maintaining system time\n");
    printf("================================\n");

    return true;
}

// ========================================
// Read current time from Pico SDK
// ========================================

void motor_rtc_print(void)
{
    if (!rtc_ready)
    {
        printf("[RTC] Clock not initialized\n");
        return;
    }

    struct tm current_time = {};

    // Read SDK clock, not external DS3231
    if (!aon_timer_get_time_calendar(&current_time))
    {
        printf("[RTC] Unable to read SDK clock\n");
        return;
    }

    printf(
        "[RTC] %04d-%02d-%02d %02d:%02d:%02d\n",
        current_time.tm_year + 1900,
        current_time.tm_mon + 1,
        current_time.tm_mday,
        current_time.tm_hour,
        current_time.tm_min,
        current_time.tm_sec
    );
}
