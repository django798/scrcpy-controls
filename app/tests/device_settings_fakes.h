/* Copyright (C) 2026 moasm */
/* SPDX-License-Identifier: Apache-2.0 */
#ifndef SC_TEST_DEVICE_SETTINGS_FAKES_H
#define SC_TEST_DEVICE_SETTINGS_FAKES_H

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

struct device_settings_fake {
    atomic_bool block;
    bool fail_read;
    bool reject_write;
    bool clamp_timeout;
    unsigned calls;
    char serial[128];
    uint32_t timeout_ms;
    uint32_t stay_awake;
};

extern struct device_settings_fake device_fake;

void
device_settings_fake_reset(void);

#endif
