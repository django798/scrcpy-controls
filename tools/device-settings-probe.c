/* Copyright (C) 2026 moasm */
/* SPDX-License-Identifier: Apache-2.0 */
// Development harness: uses the same worker/capture/parser as the panel.
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>

#include "adb/adb.h"
#include "device_settings.h"
#include "util/net.h"

const char *
sc_adb_get_executable(void) {
    const char *adb = getenv("ADB");
    return adb ? adb : "adb";
}

int
main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: device_settings_probe SERIAL info|1|3|5|30|always\n");
        return 2;
    }
    static const struct {
        const char *name;
        enum sc_device_action action;
    } choices[] = {
        {"info", SC_DEVICE_ACTION_READ}, {"1", SC_DEVICE_ACTION_TIMEOUT_1},
        {"3", SC_DEVICE_ACTION_TIMEOUT_3}, {"5", SC_DEVICE_ACTION_TIMEOUT_5},
        {"30", SC_DEVICE_ACTION_TIMEOUT_30}, {"always", SC_DEVICE_ACTION_ALWAYS},
    };
    enum sc_device_action action = SC_DEVICE_ACTION_NONE;
    for (size_t i = 0; i < ARRAY_LEN(choices); ++i) {
        if (!strcmp(argv[2], choices[i].name)) {
            action = choices[i].action;
        }
    }
    if (action == SC_DEVICE_ACTION_NONE || !SDL_Init(SDL_INIT_EVENTS) || !net_init()) {
        return 2;
    }
    struct sc_device_settings settings;
    if (!sc_device_settings_init(&settings, argv[1])) {
        return 2;
    }
    if (!sc_device_settings_request(&settings, action)) {
        sc_device_settings_destroy(&settings);
        return 2;
    }
    struct sc_device_info info;
    enum sc_device_result result;
    while (!sc_device_settings_poll(&settings, &info, &result)) {
        SDL_Delay(10);
    }
    char text[192];
    sc_device_info_format(&info, text, sizeof(text));
    printf("%s\ntimeout_ms=%u stay_awake=%u fields=%u result=%u\n",
           text, info.timeout_ms, info.stay_awake, info.fields, result);
    sc_device_settings_destroy(&settings);
    net_cleanup();
    SDL_Quit();
    return result == SC_DEVICE_RESULT_OK ? 0 : 1;
}
