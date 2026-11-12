/* Copyright (C) 2026 moasm */
/* SPDX-License-Identifier: Apache-2.0 */
#include "common.h"

#include "device_settings_fakes.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>

#include "adb/adb.h"

struct device_settings_fake device_fake;

void
device_settings_fake_reset(void) {
    memset(&device_fake, 0, sizeof(device_fake));
    atomic_init(&device_fake.block, false);
    device_fake.timeout_ms = 2000;
}

bool
sc_intr_init(struct sc_intr *intr) {
    atomic_init(&intr->interrupted, false);
    return true;
}

void
sc_intr_destroy(struct sc_intr *intr) {
    (void) intr;
}

void
sc_intr_interrupt(struct sc_intr *intr) {
    atomic_store(&intr->interrupted, true);
}

bool
sc_adb_shell_capture(struct sc_intr *intr, const char *serial, const char *script,
                      char *output, size_t capacity) {
    ++device_fake.calls;
    snprintf(device_fake.serial, sizeof(device_fake.serial), "%s", serial);
    while (atomic_load(&device_fake.block) && !sc_intr_is_interrupted(intr)) {
        SDL_Delay(1);
    }
    if (device_fake.fail_read || sc_intr_is_interrupted(intr)) {
        return false;
    }
    const char *put = strstr(script, "settings put system screen_off_timeout ");
    if (put && !device_fake.reject_write) {
        uint32_t timeout, awake;
        assert(sscanf(put, "settings put system screen_off_timeout %" SCNu32
                      " && settings put global stay_on_while_plugged_in %" SCNu32,
                      &timeout, &awake) == 2);
        device_fake.timeout_ms = device_fake.clamp_timeout ? 15000 : timeout;
        device_fake.stay_awake = awake;
    }
    snprintf(output, capacity,
             "\n__SC_BATTERY__\n  Charge counter: 1234000\n  level: 72\n  scale: 100\n"
             "__SC_CAPACITY__\n3200000\n"
             "__SC_STORAGE__\nFilesystem 1K-blocks Used Available Use%% Mounted on\n"
             "/dev/block/dm-8 50086764 5458312 44497380 11%% /data/user/0\n"
             "__SC_MEMORY__\nMemTotal: 3871180 kB\nMemAvailable: 2771844 kB\n"
             "__SC_TIMEOUT__\n%" PRIu32 "\n__SC_AWAKE__\n%" PRIu32
             "\n__SC_WRITE__\n%d\n", device_fake.timeout_ms,
             device_fake.stay_awake, !device_fake.reject_write);
    return true;
}
