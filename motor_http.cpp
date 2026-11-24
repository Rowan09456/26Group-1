
#include <cstdio>
#include <cstring>
#include <string>
#include <atomic>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/cyw43_arch.h"
#include "pico/util/queue.h"

#include "motor_http.h"
#include "AsyncHttpClient.h"

// Allow the project to compile without private credentials
#if __has_include("secrets.h")
#include "secrets.h"
#define MOTOR_WIFI_CONFIGURED 1
#else
#define MOTOR_WIFI_CONFIGURED 0
#endif

#define HTTP_PORT          5000
#define REPORT_INTERVAL_MS 1000
#define WIFI_RETRY_MS      5000
#define WIFI_TIMEOUT_MS    30000

static queue_t motor_queue;

static std::atomic<bool> wifi_connected{false};

static AsyncHttpClient http;

static uint32_t last_report_ms = 0;
static bool initialized = false;


// Convert motor state into readable text
static const char *http_state_name(motor_state_t state)
{
    switch (state)
    {
        case MOTOR_IDLE:
            return "IDLE";

        case MOTOR_STARTUP:
            return "STARTUP";

        case MOTOR_RUNNING:
            return "RUNNING";

        case MOTOR_SHUTDOWN:
            return "SHUTDOWN";

        case MOTOR_ESTOP:
            return "E-STOP";

        default:
            return "UNKNOWN";
    }
}


// Core 1: Wi-Fi and HTTP communication
static void motor_http_core1(void)
{
#if MOTOR_WIFI_CONFIGURED

    printf("\n[HTTP] Core 1 started\n");

    if (cyw43_arch_init())
    {
        printf("[HTTP] Wi-Fi initialization failed\n");
        return;
    }

    cyw43_arch_enable_sta_mode();

    motor_data_t snapshot = {};
    bool have_snapshot = false;

    char url[256];

    while (true)
    {
        // Connect or reconnect to Wi-Fi
        if (cyw43_tcpip_link_status(
                &cyw43_state,
                CYW43_ITF_STA) != CYW43_LINK_UP)
        {
            wifi_connected.store(false);

            printf("[HTTP] Connecting to Wi-Fi...\n");

            int result =
                cyw43_arch_wifi_connect_timeout_ms(
                    WIFI_SSID,
                    WIFI_PASSWORD,
                    CYW43_AUTH_WPA2_AES_PSK,
                    WIFI_TIMEOUT_MS
                );

            if (result != 0)
            {
                printf(
                    "[HTTP] Wi-Fi failed: %d\n",
                    result
                );

                sleep_ms(WIFI_RETRY_MS);
                continue;
            }

            printf("[HTTP] Wi-Fi connected\n");
            printf(
                "[HTTP] Server: %s:%d\n",
                HTTP_SERVER,
                HTTP_PORT
            );
        }

        wifi_connected.store(true);

        // Retrieve the latest available motor snapshot
        motor_data_t incoming;

        while (queue_try_remove(
                   &motor_queue,
                   &incoming))
        {
            snapshot = incoming;
            have_snapshot = true;
        }

        bool ready = false;
        bool failed = false;
        bool completed = false;

        std::string response;

        // Protect HTTP state from background callbacks
        cyw43_arch_lwip_begin();

        if (http.state ==
            AsyncHttpClient::State::DONE)
        {
            response = http.response;

            http.state =
                AsyncHttpClient::State::IDLE;

            completed = true;
        }
        else if (http.state ==
                 AsyncHttpClient::State::ERROR)
        {
            http.state =
                AsyncHttpClient::State::IDLE;

            failed = true;
        }

        ready =
            (http.state ==
             AsyncHttpClient::State::IDLE);

        cyw43_arch_lwip_end();

        if (completed)
        {
            if (response.compare(
                    0, 12, "HTTP/1.1 200") == 0 ||
                response.compare(
                    0, 12, "HTTP/1.0 200") == 0)
            {
                printf("[HTTP] Report accepted\n");
            }
            else
            {
                printf(
                    "[HTTP] Server response:\n%s\n",
                    response.c_str()
                );
            }
        }

        if (failed)
        {
            printf("[HTTP] Request failed\n");
        }

        // Send actual motor readings to Flask
        if (ready && have_snapshot)
        {
            snprintf(
                url,
                sizeof(url),
                "/api/report?"
                "state=%s"
                "&frequency=%.2f"
                "&rpm=%.1f"
                "&direction=%s"
                "&estop=%d",

                http_state_name(snapshot.state),

                snapshot.frequency_hz,

                snapshot.rpm,

                snapshot.reverse_direction
                    ? "REV" : "FWD",

                snapshot.emergency_stop
                    ? 1 : 0
            );

            printf("[HTTP] Sending: %s\n", url);

            if (http.get(
                    HTTP_SERVER,
                    url,
                    HTTP_PORT))
            {
                have_snapshot = false;
            }
            else
            {
                printf(
                    "[HTTP] Unable to start request\n"
                );
            }
        }

        sleep_ms(20);
    }

#else

    printf("[HTTP] secrets.h not found\n");
    printf("[HTTP] Remote reporting disabled\n");

#endif
}


// Initialize HTTP module from Core 0
extern "C" void motor_http_init(void)
{
    if (initialized)
    {
        return;
    }

#if MOTOR_WIFI_CONFIGURED

    queue_init(
        &motor_queue,
        sizeof(motor_data_t),
        1
    );

    initialized = true;

    multicore_launch_core1(motor_http_core1);

    printf("[HTTP] Reporting module initialized\n");

#else

    printf("[HTTP] Wi-Fi credentials not configured\n");

#endif
}


// Called by Core 0 during normal motor operation
extern "C" void motor_http_update(
    const motor_data_t *data)
{
    if (!initialized || data == nullptr)
    {
        return;
    }

    uint32_t now =
        to_ms_since_boot(get_absolute_time());

    if (now - last_report_ms <
        REPORT_INTERVAL_MS)
    {
        return;
    }

    last_report_ms = now;

    // Copy the motor data instead of sharing a pointer
    motor_data_t snapshot = *data;

    // Never wait for Core 1
    if (!queue_try_add(
            &motor_queue,
            &snapshot))
    {
        motor_data_t old_snapshot;

        queue_try_remove(
            &motor_queue,
            &old_snapshot
        );

        queue_try_add(
            &motor_queue,
            &snapshot
        );
    }
}


// Return current Wi-Fi connection status
extern "C" bool motor_http_is_connected(void)
{
    return wifi_connected.load();
}
