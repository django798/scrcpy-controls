/* Copyright (C) 2026 moasm */
/* SPDX-License-Identifier: Apache-2.0 */
#ifndef SC_DEVICE_SETTINGS_H
#define SC_DEVICE_SETTINGS_H

#include "common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "util/intr.h"
#include "util/thread.h"

enum sc_device_action {
    SC_DEVICE_ACTION_NONE,
    SC_DEVICE_ACTION_READ,
    SC_DEVICE_ACTION_TIMEOUT_1,
    SC_DEVICE_ACTION_TIMEOUT_3,
    SC_DEVICE_ACTION_TIMEOUT_5,
    SC_DEVICE_ACTION_TIMEOUT_30,
    SC_DEVICE_ACTION_ALWAYS,
};

enum sc_device_info_field {
    SC_DEVICE_INFO_BATTERY = 1 << 0,
    SC_DEVICE_INFO_CAPACITY = 1 << 1,
    SC_DEVICE_INFO_STORAGE = 1 << 2,
    SC_DEVICE_INFO_MEMORY_TOTAL = 1 << 3,
    SC_DEVICE_INFO_MEMORY_AVAILABLE = 1 << 4,
    SC_DEVICE_INFO_TIMEOUT = 1 << 5,
    SC_DEVICE_INFO_STAY_AWAKE = 1 << 6,
};

struct sc_device_info {
    unsigned fields;
    unsigned battery_percent;
    unsigned battery_mah; // full/design charge capacity, never charge counter
    uint64_t storage_total_kib;
    uint64_t storage_used_kib;
    uint64_t memory_total_kib;
    uint64_t memory_available_kib;
    uint32_t timeout_ms;
    uint32_t stay_awake;
};

enum sc_device_result {
    SC_DEVICE_RESULT_OK,
    SC_DEVICE_RESULT_READ_FAILED,
    SC_DEVICE_RESULT_SET_FAILED,
    SC_DEVICE_RESULT_TIMEOUT,
};

// One worker per in-flight request. Only done/info/result are shared with the
// worker (protected by mutex). Other members belong to the window's UI thread.
struct sc_device_settings {
    char *serial;
    sc_mutex mutex;
    struct sc_intr intr;
    sc_thread thread;
    bool running;
    bool stopped;
    bool done;
    bool timed_out;
    sc_tick deadline;
    enum sc_device_action action;
    struct sc_device_info info;
    enum sc_device_result result;
};

bool
sc_device_info_parse(char *output, struct sc_device_info *info, bool *write_ok);

void
sc_device_info_format(const struct sc_device_info *info, char *buf, size_t len);

bool
sc_device_info_matches(const struct sc_device_info *info,
                        enum sc_device_action action);

bool
sc_device_settings_init(struct sc_device_settings *settings, const char *serial);

bool
sc_device_settings_request(struct sc_device_settings *settings,
                            enum sc_device_action action);

// Poll from the UI thread while a request is running. Enforces a five-second
// timeout. Returns true once a result was consumed and the worker joined.
bool
sc_device_settings_poll(struct sc_device_settings *settings,
                         struct sc_device_info *info,
                         enum sc_device_result *result);

void
sc_device_settings_stop(struct sc_device_settings *settings);

void
sc_device_settings_destroy(struct sc_device_settings *settings);

#endif
