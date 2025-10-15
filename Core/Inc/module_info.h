#ifndef MODULE_INFO_H
#define MODULE_INFO_H

#include <stddef.h>

#ifndef MODULE_NAME
#define MODULE_NAME "OpenGrader Module"
#endif

#ifndef MODULE_FIRMWARE_VERSION_MAJOR
#define MODULE_FIRMWARE_VERSION_MAJOR 1
#endif

#ifndef MODULE_FIRMWARE_VERSION_MINOR
#define MODULE_FIRMWARE_VERSION_MINOR 0
#endif

#ifndef MODULE_FIRMWARE_VERSION_PATCH
#define MODULE_FIRMWARE_VERSION_PATCH 0
#endif

// Copy the module name into a fixed-size buffer, ensuring null termination
static inline void module_info_copy_name(char *dest, size_t max_len)
{
    if (!dest || max_len == 0) {
        return;
    }

    const char *src = MODULE_NAME;
    size_t idx = 0;

    while (src[idx] != '\0' && idx < max_len - 1u) {
        dest[idx] = src[idx];
        idx++;
    }

    dest[idx] = '\0';

    for (size_t pad = idx + 1u; pad < max_len; ++pad) {
        dest[pad] = '\0';
    }
}

#endif /* MODULE_INFO_H */
