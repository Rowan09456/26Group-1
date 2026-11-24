
#ifndef MOTOR_HTTP_H
#define MOTOR_HTTP_H

#include <stdbool.h>
#include "motor_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize HTTP reporting and launch Core 1
void motor_http_init(void);

// Send a snapshot of the current motor data
void motor_http_update(const motor_data_t *data);

// Check Wi-Fi connection status
bool motor_http_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif
