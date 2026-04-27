/*
 * direct_wheel_abi.h — debug-only C ABI for external tools.
 *
 * This header is NOT used by Cyberpunk 2077 or the game process. It exists so
 * that standalone test tools can poll the plugin's device state via a flat C
 * surface when desired (e.g. a CLI that prints live steering / throttle /
 * brake). The game uses the redscript native surface instead.
 *
 * Stable across v0.x — breaking changes will bump DIRECT_WHEEL_ABI_VERSION.
 */

#pragma once

#include <stdint.h>

#define DIRECT_WHEEL_ABI_VERSION 1

#define DIRECT_WHEEL_OK              0
#define DIRECT_WHEEL_E_NOT_INIT     -1
#define DIRECT_WHEEL_E_NO_DEVICE    -2
#define DIRECT_WHEEL_E_NO_FFB       -3
#define DIRECT_WHEEL_E_DEVICE_LOST  -4
#define DIRECT_WHEEL_E_BAD_ARG      -5

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t vendor_id;
    uint32_t product_id;
    char     product_name[128];
    uint8_t  model_id;
    uint8_t  has_ffb;
    uint8_t  has_clutch;
    uint8_t  has_shifter;
    uint8_t  num_axes;
    uint8_t  num_buttons;
    uint16_t steering_range_deg;
} direct_wheel_caps_t;

typedef struct {
    float    steer;
    float    throttle;
    float    brake;
    float    clutch;
    uint32_t buttons_lo;
    uint32_t buttons_hi;
    int8_t   shifter_gear;
    uint8_t  connected;
} direct_wheel_state_t;

#ifdef __cplusplus
} /* extern "C" */
#endif
