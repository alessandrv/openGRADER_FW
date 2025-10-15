#ifndef DEVICE_INFO_UTIL_H
#define DEVICE_INFO_UTIL_H

#include <stddef.h>
#include <string.h>

#include "config_protocol.h"
#include "module_info.h"
#include "pin_config.h"
#include "input/keymap.h"

static inline void device_info_populate(device_info_t *info, uint8_t device_type, uint8_t i2c_devices)
{
    if (!info) {
        return;
    }

    info->protocol_version = CONFIG_PROTOCOL_VERSION;
    info->firmware_version_major = MODULE_FIRMWARE_VERSION_MAJOR;
    info->firmware_version_minor = MODULE_FIRMWARE_VERSION_MINOR;
    info->firmware_version_patch = MODULE_FIRMWARE_VERSION_PATCH;
    info->device_type = device_type;
    info->matrix_rows = MATRIX_ROWS;
    info->matrix_cols = MATRIX_COLS;
    info->encoder_count = ENCODER_COUNT;
    info->layer_count = KEYMAP_LAYER_COUNT;
    info->i2c_devices = i2c_devices;

    module_info_copy_name(info->device_name, sizeof(info->device_name));

    memset(info->reserved, 0, sizeof(info->reserved));
}

#endif /* DEVICE_INFO_UTIL_H */
