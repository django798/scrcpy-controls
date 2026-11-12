/* Copyright (C) 2026 moasm */
/* SPDX-License-Identifier: Apache-2.0 */
#include "device_settings.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "adb/adb.h"
#include "util/log.h"

#define ALWAYS_TIMEOUT UINT32_C(2147483647)
#define ALWAYS_POWER_SOURCES 7 // AC, USB and wireless power
#define REQUEST_TIMEOUT SC_TICK_FROM_SEC(5)

static bool
number(const char *s, uint64_t *value) {
    while (isspace((unsigned char) *s)) {
        ++s;
    }
    if (!isdigit((unsigned char) *s)) {
        return false;
    }
    errno = 0;
    char *end;
    unsigned long long n = strtoull(s, &end, 10);
    if (errno == ERANGE || (*end && !isspace((unsigned char) *end))) {
        return false;
    }
    *value = n;
    return true;
}

bool
sc_device_info_parse(char *output, struct sc_device_info *info, bool *write_ok) {
    *info = (struct sc_device_info) {0};
    *write_ok = false;
    enum {NONE, BATTERY, CAPACITY, STORAGE, MEMORY, TIMEOUT, AWAKE, WRITE} section = NONE;
    uint64_t level = UINT64_MAX;
    uint64_t scale = 0;
    for (char *line = output; line && *line;) {
        char *next = strchr(line, '\n');
        if (next) {
            *next++ = '\0';
        }
        while (isspace((unsigned char) *line)) {
            ++line;
        }
        size_t len = strlen(line);
        while (len && isspace((unsigned char) line[len - 1])) {
            line[--len] = '\0';
        }
        if (!strcmp(line, "__SC_BATTERY__")) {
            section = BATTERY;
        } else if (!strcmp(line, "__SC_CAPACITY__")) {
            section = CAPACITY;
        } else if (!strcmp(line, "__SC_STORAGE__")) {
            section = STORAGE;
        } else if (!strcmp(line, "__SC_MEMORY__")) {
            section = MEMORY;
        } else if (!strcmp(line, "__SC_TIMEOUT__")) {
            section = TIMEOUT;
        } else if (!strcmp(line, "__SC_AWAKE__")) {
            section = AWAKE;
        } else if (!strcmp(line, "__SC_WRITE__")) {
            section = WRITE;
        } else {
            uint64_t n;
            switch (section) {
                case BATTERY:
                    if (!strncmp(line, "level:", 6) && number(line + 6, &n)) {
                        level = n;
                    } else if (!strncmp(line, "scale:", 6) && number(line + 6, &n)) {
                        scale = n;
                    }
                    break;
                case CAPACITY:
                    if (!(info->fields & SC_DEVICE_INFO_CAPACITY)
                            && number(line, &n) && n >= 1000 && n <= UINT32_MAX) {
                        info->battery_mah = n / 1000; // sysfs charge is in uAh
                        info->fields |= SC_DEVICE_INFO_CAPACITY;
                    }
                    break;
                case STORAGE: {
                    uint64_t total, used, available;
                    int count = sscanf(line, "%*s %" SCNu64 " %" SCNu64 " %" SCNu64,
                                        &total, &used, &available);
                    if (isdigit((unsigned char) *line)) {
                        count = sscanf(line, "%" SCNu64 " %" SCNu64 " %" SCNu64,
                                       &total, &used, &available);
                    }
                    if (count == 3 && total && used <= total && available <= total) {
                        info->storage_total_kib = total;
                        info->storage_used_kib = used;
                        info->fields |= SC_DEVICE_INFO_STORAGE;
                    }
                    break;
                }
                case MEMORY:
                    if (!strncmp(line, "MemTotal:", 9)
                            && number(line + 9, &n) && n) {
                        info->memory_total_kib = n;
                        info->fields |= SC_DEVICE_INFO_MEMORY_TOTAL;
                    } else if (!strncmp(line, "MemAvailable:", 13)
                            && number(line + 13, &n)) {
                        info->memory_available_kib = n;
                        info->fields |= SC_DEVICE_INFO_MEMORY_AVAILABLE;
                    }
                    break;
                case TIMEOUT:
                    if (number(line, &n) && n <= ALWAYS_TIMEOUT) {
                        info->timeout_ms = n;
                        info->fields |= SC_DEVICE_INFO_TIMEOUT;
                    }
                    break;
                case AWAKE:
                    if (number(line, &n) && n <= UINT32_MAX) {
                        info->stay_awake = n;
                        info->fields |= SC_DEVICE_INFO_STAY_AWAKE;
                    }
                    break;
                case WRITE:
                    *write_ok = !strcmp(line, "1");
                    break;
                default:
                    break;
            }
        }
        line = next;
    }
    if (scale && scale <= 1000000 && level <= scale) {
        info->battery_percent = (level * 100 + scale / 2) / scale;
        info->fields |= SC_DEVICE_INFO_BATTERY;
    }
    if (!(info->fields & SC_DEVICE_INFO_MEMORY_TOTAL)
            || info->memory_available_kib > info->memory_total_kib) {
        info->fields &= ~SC_DEVICE_INFO_MEMORY_AVAILABLE;
    }
    return info->fields != 0;
}

static bool
action_values(enum sc_device_action action, uint32_t *timeout, uint32_t *awake) {
    *awake = 0;
    switch (action) {
        case SC_DEVICE_ACTION_TIMEOUT_1: *timeout = 60000; return true;
        case SC_DEVICE_ACTION_TIMEOUT_3: *timeout = 180000; return true;
        case SC_DEVICE_ACTION_TIMEOUT_5: *timeout = 300000; return true;
        case SC_DEVICE_ACTION_TIMEOUT_30: *timeout = 1800000; return true;
        case SC_DEVICE_ACTION_ALWAYS:
            *timeout = ALWAYS_TIMEOUT;
            *awake = ALWAYS_POWER_SOURCES;
            return true;
        default:
            return false;
    }
}

bool
sc_device_info_matches(const struct sc_device_info *info,
                        enum sc_device_action action) {
    uint32_t timeout, awake;
    unsigned required = SC_DEVICE_INFO_TIMEOUT | SC_DEVICE_INFO_STAY_AWAKE;
    return (info->fields & required) == required
        && action_values(action, &timeout, &awake)
        && info->timeout_ms == timeout && info->stay_awake == awake;
}

void
sc_device_info_format(const struct sc_device_info *info, char *buf, size_t len) {
    char battery[40] = "Bat --";
    char data[48] = "Data --";
    char memory[48] = "RAM --";
    char timeout[32] = "Off --";
    const double gib = 1024.0 * 1024;
    if (info->fields & SC_DEVICE_INFO_BATTERY) {
        if (info->fields & SC_DEVICE_INFO_CAPACITY) {
            snprintf(battery, sizeof(battery), "Bat %u%%/%umAh",
                     info->battery_percent, info->battery_mah);
        } else {
            snprintf(battery, sizeof(battery), "Bat %u%%", info->battery_percent);
        }
    }
    if (info->fields & SC_DEVICE_INFO_STORAGE) {
        snprintf(data, sizeof(data), "Data(/data) %.1f/%.1f",
                 info->storage_used_kib / gib, info->storage_total_kib / gib);
    }
    if (info->fields & SC_DEVICE_INFO_MEMORY_AVAILABLE) {
        snprintf(memory, sizeof(memory), "RAM %.1f/%.1f",
                 (info->memory_total_kib - info->memory_available_kib) / gib,
                 info->memory_total_kib / gib);
    } else if (info->fields & SC_DEVICE_INFO_MEMORY_TOTAL) {
        snprintf(memory, sizeof(memory), "RAM --/%.1f", info->memory_total_kib / gib);
    }
    if (info->fields & SC_DEVICE_INFO_TIMEOUT) {
        if (info->timeout_ms == ALWAYS_TIMEOUT) {
            snprintf(timeout, sizeof(timeout), "Off Always");
        } else if (info->timeout_ms && !(info->timeout_ms % 60000)) {
            snprintf(timeout, sizeof(timeout), "Off %um", info->timeout_ms / 60000);
        } else {
            snprintf(timeout, sizeof(timeout), "Off %us", info->timeout_ms / 1000);
        }
    }
    snprintf(buf, len, "%s | %s | %s | %s", battery, data, memory, timeout);
}

static const char query[] =
    "printf '\n__SC_BATTERY__\n'; dumpsys battery; "
    "printf '\n__SC_CAPACITY__\n'; "
    "for f in /sys/class/power_supply/battery/charge_full_design "
             "/sys/class/power_supply/battery/charge_full; do "
        "if [ -r \"$f\" ]; then cat \"$f\" 2>/dev/null; fi; done; "
    "printf '\n__SC_STORAGE__\n'; df -k /data; "
    "printf '\n__SC_MEMORY__\n'; "
    "grep -E '^(MemTotal|MemAvailable):' /proc/meminfo; "
    "printf '\n__SC_TIMEOUT__\n'; settings get system screen_off_timeout; "
    "printf '\n__SC_AWAKE__\n'; settings get global stay_on_while_plugged_in; "
    "printf '\n__SC_WRITE__\n%s\n' \"$sc_set_ok\"";

static int
run_request(void *userdata) {
    struct sc_device_settings *settings = userdata;
    char command[2048];
    uint32_t timeout, awake;
    if (action_values(settings->action, &timeout, &awake)) {
        snprintf(command, sizeof(command),
                 "sc_set_ok=0; if settings put system screen_off_timeout %" PRIu32
                 " && settings put global stay_on_while_plugged_in %" PRIu32
                 "; then sc_set_ok=1; fi; %s", timeout, awake, query);
    } else {
        snprintf(command, sizeof(command), "sc_set_ok=1; %s", query);
    }
    char output[8192];
    struct sc_device_info info = {0};
    bool write_ok;
    bool ok = sc_adb_shell_capture(&settings->intr, settings->serial, command,
                                   output, sizeof(output));
    ok = ok && sc_device_info_parse(output, &info, &write_ok);
    enum sc_device_result result;
    if (settings->action == SC_DEVICE_ACTION_READ) {
        result = ok ? SC_DEVICE_RESULT_OK : SC_DEVICE_RESULT_READ_FAILED;
    } else {
        result = ok && write_ok && sc_device_info_matches(&info, settings->action)
               ? SC_DEVICE_RESULT_OK : SC_DEVICE_RESULT_SET_FAILED;
    }
    sc_mutex_lock(&settings->mutex);
    settings->info = info;
    settings->result = result;
    settings->done = true;
    sc_mutex_unlock(&settings->mutex);
    return 0;
}

bool
sc_device_settings_init(struct sc_device_settings *settings, const char *serial) {
    *settings = (struct sc_device_settings) {0};
    settings->serial = strdup(serial);
    if (!settings->serial) {
        LOG_OOM();
        return false;
    }
    if (!sc_mutex_init(&settings->mutex)) {
        free(settings->serial);
        return false;
    }
    return true;
}

bool
sc_device_settings_request(struct sc_device_settings *settings,
                            enum sc_device_action action) {
    if (settings->running || settings->stopped || action < SC_DEVICE_ACTION_READ
            || action > SC_DEVICE_ACTION_ALWAYS) {
        return false;
    }
    if (!sc_intr_init(&settings->intr)) {
        return false;
    }
    settings->action = action;
    settings->done = false;
    settings->timed_out = false;
    settings->deadline = sc_tick_now() + REQUEST_TIMEOUT;
    if (!sc_thread_create(&settings->thread, run_request, "scrcpy-settings", settings)) {
        sc_intr_destroy(&settings->intr);
        return false;
    }
    settings->running = true;
    return true;
}

bool
sc_device_settings_poll(struct sc_device_settings *settings,
                         struct sc_device_info *info,
                         enum sc_device_result *result) {
    if (!settings->running) {
        return false;
    }
    sc_mutex_lock(&settings->mutex);
    bool done = settings->done;
    if (!done && !settings->timed_out && sc_tick_now() >= settings->deadline) {
        settings->timed_out = true;
        sc_intr_interrupt(&settings->intr);
    }
    if (done) {
        *info = settings->info;
        *result = settings->timed_out ? SC_DEVICE_RESULT_TIMEOUT : settings->result;
    }
    sc_mutex_unlock(&settings->mutex);
    if (!done) {
        return false;
    }
    sc_thread_join(&settings->thread, NULL);
    sc_intr_destroy(&settings->intr);
    settings->running = false;
    LOGD("Device quick settings for %s: action=%u result=%u",
         settings->serial, settings->action, *result);
    return true;
}

void
sc_device_settings_stop(struct sc_device_settings *settings) {
    settings->stopped = true;
    if (settings->running) {
        sc_intr_interrupt(&settings->intr);
        sc_thread_join(&settings->thread, NULL);
        sc_intr_destroy(&settings->intr);
        settings->running = false;
    }
}

void
sc_device_settings_destroy(struct sc_device_settings *settings) {
    sc_device_settings_stop(settings);
    sc_mutex_destroy(&settings->mutex);
    free(settings->serial);
}
