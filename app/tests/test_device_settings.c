/* Copyright (C) 2026 moasm */
/* SPDX-License-Identifier: Apache-2.0 */
#include "common.h"

#include <assert.h>
#include <string.h>
#include <SDL3/SDL.h>

#include "device_settings.h"
#include "device_settings_fakes.h"

static enum sc_device_result
wait_result(struct sc_device_settings *settings, struct sc_device_info *info) {
    sc_tick deadline = sc_tick_now() + SC_TICK_FROM_SEC(2);
    enum sc_device_result result;
    while (!sc_device_settings_poll(settings, info, &result)) {
        assert(sc_tick_now() < deadline);
        SDL_Delay(1);
    }
    return result;
}

static void
test_partial_and_malformed_info(void) {
    char output[] =
        "__SC_BATTERY__\nCharge counter: 2400000\nlevel: 128\nscale: 255\n"
        "__SC_CAPACITY__\nPermission denied\n0\n-1\nnull\n"
        "__SC_STORAGE__\nFilesystem 1K-blocks Used Available Use% Mounted on\n"
        "/very/long/filesystem\n1000 200 800 20% /data\n"
        "__SC_MEMORY__\nMemTotal: 4000 kB\nMemAvailable: 5000 kB\n"
        "__SC_TIMEOUT__\nnull\n__SC_AWAKE__\n0\n";
    struct sc_device_info info;
    bool write_ok;
    assert(sc_device_info_parse(output, &info, &write_ok));
    assert(info.battery_percent == 50);
    assert(!(info.fields & SC_DEVICE_INFO_CAPACITY)); // charge counter is not capacity
    assert(info.storage_total_kib == 1000 && info.storage_used_kib == 200);
    assert(info.fields & SC_DEVICE_INFO_MEMORY_TOTAL);
    assert(!(info.fields & SC_DEVICE_INFO_MEMORY_AVAILABLE));
    assert(!(info.fields & SC_DEVICE_INFO_TIMEOUT));
    assert(!write_ok);
    assert(!sc_device_info_matches(&info, SC_DEVICE_ACTION_TIMEOUT_1));

    char invalid[] = "__SC_BATTERY__\nlevel: 999\nscale: 100\n"
                     "__SC_TIMEOUT__\n-1\n__SC_AWAKE__\nnull\n";
    assert(!sc_device_info_parse(invalid, &info, &write_ok));
    char text[192];
    sc_device_info_format(&info, text, sizeof(text));
    assert(!strcmp(text, "Bat -- | Data -- | RAM -- | Off --"));
}

static void
test_actions_readback_and_serial(void) {
    device_settings_fake_reset();
    struct sc_device_settings settings;
    assert(sc_device_settings_init(&settings, "board-A"));
    assert(sc_device_settings_request(&settings, SC_DEVICE_ACTION_READ));
    assert(!sc_device_settings_request(&settings, SC_DEVICE_ACTION_ALWAYS));
    struct sc_device_info info;
    assert(wait_result(&settings, &info) == SC_DEVICE_RESULT_OK);
    assert(info.battery_percent == 72 && info.battery_mah == 3200);
    assert(info.timeout_ms == 2000 && info.stay_awake == 0);
    char text[192];
    sc_device_info_format(&info, text, sizeof(text));
    assert(!strcmp(text, "Bat 72%/3200mAh | Data(/data) 5.2/47.8 | RAM 1.0/3.7 | Off 2s"));
    assert(!strcmp(device_fake.serial, "board-A"));

    static const struct {
        enum sc_device_action action;
        uint32_t timeout;
        uint32_t awake;
    } cases[] = {
        {SC_DEVICE_ACTION_TIMEOUT_1, 60000, 0},
        {SC_DEVICE_ACTION_TIMEOUT_3, 180000, 0},
        {SC_DEVICE_ACTION_TIMEOUT_5, 300000, 0},
        {SC_DEVICE_ACTION_TIMEOUT_30, 1800000, 0},
        {SC_DEVICE_ACTION_ALWAYS, 2147483647, 7},
        {SC_DEVICE_ACTION_TIMEOUT_1, 60000, 0}, // leaving Always must clear stay-awake
    };
    for (size_t i = 0; i < ARRAY_LEN(cases); ++i) {
        assert(sc_device_settings_request(&settings, cases[i].action));
        assert(wait_result(&settings, &info) == SC_DEVICE_RESULT_OK);
        assert(info.timeout_ms == cases[i].timeout);
        assert(info.stay_awake == cases[i].awake);
        assert(sc_device_info_matches(&info, cases[i].action));
        assert(!strcmp(device_fake.serial, "board-A"));
    }
    sc_device_settings_destroy(&settings);

    assert(sc_device_settings_init(&settings, "board-B"));
    assert(sc_device_settings_request(&settings, SC_DEVICE_ACTION_READ));
    assert(wait_result(&settings, &info) == SC_DEVICE_RESULT_OK);
    assert(!strcmp(device_fake.serial, "board-B"));
    sc_device_settings_destroy(&settings);
}

static void
test_errors_timeout_cancel_and_retry(void) {
    device_settings_fake_reset();
    struct sc_device_settings settings;
    assert(sc_device_settings_init(&settings, "board-A"));
    struct sc_device_info info;
    device_fake.reject_write = true;
    assert(sc_device_settings_request(&settings, SC_DEVICE_ACTION_TIMEOUT_3));
    assert(wait_result(&settings, &info) == SC_DEVICE_RESULT_SET_FAILED);
    assert(info.timeout_ms == 2000);
    device_fake.reject_write = false;
    device_fake.clamp_timeout = true;
    assert(sc_device_settings_request(&settings, SC_DEVICE_ACTION_TIMEOUT_3));
    assert(wait_result(&settings, &info) == SC_DEVICE_RESULT_SET_FAILED);
    assert(info.timeout_ms == 15000); // a successful command is not an applied setting
    device_fake.clamp_timeout = false;
    device_fake.fail_read = true;
    assert(sc_device_settings_request(&settings, SC_DEVICE_ACTION_READ));
    assert(wait_result(&settings, &info) == SC_DEVICE_RESULT_READ_FAILED);
    assert(!info.fields);
    device_fake.fail_read = false;

    atomic_store(&device_fake.block, true);
    assert(sc_device_settings_request(&settings, SC_DEVICE_ACTION_READ));
    settings.deadline = 0;
    assert(wait_result(&settings, &info) == SC_DEVICE_RESULT_TIMEOUT);
    atomic_store(&device_fake.block, false);
    assert(sc_device_settings_request(&settings, SC_DEVICE_ACTION_READ));
    assert(wait_result(&settings, &info) == SC_DEVICE_RESULT_OK); // fresh interruptor
    atomic_store(&device_fake.block, true);
    assert(sc_device_settings_request(&settings, SC_DEVICE_ACTION_READ));
    sc_device_settings_stop(&settings);
    assert(!settings.running);
    assert(!sc_device_settings_request(&settings, SC_DEVICE_ACTION_READ));
    sc_device_settings_destroy(&settings);
}

int
main(void) {
    assert(SDL_Init(SDL_INIT_EVENTS));
    test_partial_and_malformed_info();
    test_actions_readback_and_serial();
    test_errors_timeout_cancel_and_retry();
    SDL_Quit();
    return 0;
}
