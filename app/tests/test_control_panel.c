/* Copyright (C) 2026 moasm */
/* SPDX-License-Identifier: Apache-2.0 */
#include "common.h"

#include <assert.h>
#include <string.h>
#include <SDL3/SDL.h>

#include "control_panel.h"
#include "events.h"
#include "device_settings_fakes.h"

struct recorder {
    struct sc_control_msg messages[128];
    unsigned count;
    bool fail;
};

static bool
record(void *userdata, const struct sc_control_msg *msg) {
    struct recorder *rec = userdata;
    if (rec->fail) {
        return false;
    }
    assert(rec->count < ARRAY_LEN(rec->messages));
    rec->messages[rec->count++] = *msg;
    return true;
}

static void
setup(struct sc_control_panel *panel, struct recorder *rec) {
    *rec = (struct recorder) {0};
    sc_control_panel_init(panel, true, record, rec);
    sc_control_panel_layout(panel, (struct sc_size) {1280, 640});
}

static bool
mouse(struct sc_control_panel *panel, bool down, uint8_t button,
       float x, float y) {
    SDL_Event event = {.button = {
        .type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP,
        .button = button,
        .x = panel->rect.x + x,
        .y = y,
    }};
    return sc_control_panel_handle_event(panel, &event);
}

static void
click(struct sc_control_panel *panel, float x, float y) {
    assert(mouse(panel, true, SDL_BUTTON_LEFT, x, y));
    assert(mouse(panel, false, SDL_BUTTON_LEFT, x, y));
}

static void
check(const struct recorder *rec, unsigned index, enum android_keycode code,
       enum android_keyevent_action action, uint32_t mods) {
    assert(index < rec->count);
    const struct sc_control_msg *msg = &rec->messages[index];
    assert(msg->type == SC_CONTROL_MSG_TYPE_INJECT_KEYCODE);
    assert(msg->inject_keycode.keycode == code);
    assert(msg->inject_keycode.action == action);
    assert(msg->inject_keycode.metastate == mods);
}

static void
test_press_drag_release(void) {
    struct sc_control_panel panel;
    struct recorder rec;
    setup(&panel, &rec);
    assert(mouse(&panel, true, SDL_BUTTON_LEFT, 60, 80)); // Power
    assert(rec.count == 1);
    assert(!panel.timer); // holding Power must never toggle it repeatedly
    check(&rec, 0, AKEYCODE_POWER, AKEY_EVENT_ACTION_DOWN, 0);
    assert(mouse(&panel, false, SDL_BUTTON_LEFT, -200, 100)); // over video
    assert(rec.count == 2);
    check(&rec, 1, AKEYCODE_POWER, AKEY_EVENT_ACTION_UP, 0);
    assert(panel.pressed == -1);

    assert(!mouse(&panel, true, SDL_BUTTON_LEFT, -50, 80)); // video drag
    assert(!mouse(&panel, false, SDL_BUTTON_LEFT, 60, 80)); // video gets UP
    assert(mouse(&panel, true, SDL_BUTTON_RIGHT, 60, 80));
    assert(mouse(&panel, false, SDL_BUTTON_RIGHT, 60, 80));
    click(&panel, 10, 10); // panel background consumes the click
    assert(rec.count == 2);
    sc_control_panel_release_all(&panel);
}

static void
test_modifiers_and_focus(void) {
    struct sc_control_panel panel;
    struct recorder rec;
    setup(&panel, &rec);
    click(&panel, 40, 550); // left Ctrl latches across mouse UP
    uint32_t ctrl = AMETA_CTRL_LEFT_ON | AMETA_CTRL_ON;
    assert(rec.count == 1);
    check(&rec, 0, AKEYCODE_CTRL_LEFT, AKEY_EVENT_ACTION_DOWN, ctrl);
    click(&panel, 105, 478); // A -> Ctrl+A
    check(&rec, 1, AKEYCODE_A, AKEY_EVENT_ACTION_DOWN, ctrl);
    check(&rec, 2, AKEYCODE_A, AKEY_EVENT_ACTION_UP, ctrl);
    click(&panel, 40, 550); // release Ctrl
    check(&rec, 3, AKEYCODE_CTRL_LEFT, AKEY_EVENT_ACTION_UP, 0);

    click(&panel, 40, 514); // left Shift
    click(&panel, 580, 514); // right Shift
    click(&panel, 40, 514); // release left, right must still be active
    uint32_t shift = AMETA_SHIFT_RIGHT_ON | AMETA_SHIFT_ON;
    check(&rec, 6, AKEYCODE_SHIFT_LEFT, AKEY_EVENT_ACTION_UP, shift);
    assert(mouse(&panel, true, SDL_BUTTON_LEFT, 78, 406)); // 1, held with Shift
    check(&rec, 7, AKEYCODE_1, AKEY_EVENT_ACTION_DOWN, shift);

    SDL_Event lost = {.type = SDL_EVENT_WINDOW_FOCUS_LOST};
    assert(!sc_control_panel_handle_event(&panel, &lost));
    check(&rec, 8, AKEYCODE_1, AKEY_EVENT_ACTION_UP, shift);
    check(&rec, 9, AKEYCODE_SHIFT_RIGHT, AKEY_EVENT_ACTION_UP, 0);
    assert(!panel.modifiers && !panel.timer && panel.pressed == -1);
    assert(!panel.captured_buttons);
    sc_control_panel_release_all(&panel);
    assert(rec.count == 10); // cleanup must be idempotent
}

static void
test_lock_repeat_and_disconnect(void) {
    struct sc_control_panel panel;
    struct recorder rec;
    setup(&panel, &rec);
    click(&panel, 40, 478); // Caps
    check(&rec, 0, AKEYCODE_CAPS_LOCK, AKEY_EVENT_ACTION_DOWN, AMETA_CAPS_LOCK_ON);
    check(&rec, 1, AKEYCODE_CAPS_LOCK, AKEY_EVENT_ACTION_UP, AMETA_CAPS_LOCK_ON);
    assert(mouse(&panel, true, SDL_BUTTON_LEFT, 95, 215)); // D-pad Up
    assert(panel.timer);
    SDL_Event tick = {.type = SC_EVENT_CONTROL_PANEL_TICK};
    panel.next_repeat = 0;
    assert(sc_control_panel_handle_event(&panel, &tick));
    check(&rec, 3, AKEYCODE_DPAD_UP, AKEY_EVENT_ACTION_DOWN, AMETA_CAPS_LOCK_ON);
    assert(rec.messages[3].inject_keycode.repeat == 1);
    assert(mouse(&panel, false, SDL_BUTTON_LEFT, 95, 215));
    check(&rec, 4, AKEYCODE_DPAD_UP, AKEY_EVENT_ACTION_UP, AMETA_CAPS_LOCK_ON);
    assert(!panel.timer);
    assert(sc_control_panel_handle_event(&panel, &tick));
    assert(rec.count == 5); // an already queued tick must be harmless
    panel.repeat_enabled = false;
    assert(mouse(&panel, true, SDL_BUTTON_LEFT, 95, 215));
    assert(!panel.timer);
    panel.next_repeat = 0;
    assert(sc_control_panel_handle_event(&panel, &tick));
    assert(rec.count == 6); // --no-key-repeat also applies to panel keys
    assert(mouse(&panel, false, SDL_BUTTON_LEFT, 95, 215));
    sc_control_panel_disconnect(&panel);
    click(&panel, 60, 80);
    assert(rec.count == 7);
}

static void
test_scaling_touch_and_disabled(void) {
    struct sc_control_panel panel;
    struct recorder rec;
    setup(&panel, &rec);
    sc_control_panel_layout(&panel, (struct sc_size) {640, 320});
    assert(panel.rect.x == 320 && panel.rect.w == 320);
    SDL_Event touch = {.tfinger = {
        .type = SDL_EVENT_FINGER_DOWN, .fingerID = 42,
        .x = (320.f + 30) / 640, .y = 40.f / 320,
    }};
    assert(sc_control_panel_handle_event(&panel, &touch));
    check(&rec, 0, AKEYCODE_POWER, AKEY_EVENT_ACTION_DOWN, 0);
    touch.type = SDL_EVENT_FINGER_UP;
    touch.tfinger.x = 0.1f; // release on video
    assert(sc_control_panel_handle_event(&panel, &touch));
    check(&rec, 1, AKEYCODE_POWER, AKEY_EVENT_ACTION_UP, 0);

    // At portrait sizes, the panel must leave at least 40% for video.
    sc_control_panel_layout(&panel, (struct sc_size) {400, 800});
    assert(panel.rect.w == 240 && panel.rect.x == 160);
    sc_control_panel_disconnect(&panel);
    sc_control_panel_init(&panel, false, NULL, NULL);
    SDL_Event down = {.type = SDL_EVENT_MOUSE_BUTTON_DOWN};
    assert(!sc_control_panel_handle_event(&panel, &down));
    sc_control_panel_release_all(&panel);
}

static void
test_failed_enqueue(void) {
    struct sc_control_panel panel;
    struct recorder rec;
    setup(&panel, &rec);
    rec.fail = true;
    click(&panel, 40, 550); // Ctrl must not appear latched if DOWN was rejected
    assert(!panel.modifiers);
    click(&panel, 60, 80); // do not send an unmatched UP after rejected DOWN
    assert(panel.pressed == -1 && !panel.timer && !rec.count);
    sc_control_panel_release_all(&panel);
}

static void
wait_settings(struct sc_control_panel *panel) {
    SDL_Event tick = {.type = SC_EVENT_DEVICE_SETTINGS_TICK};
    sc_tick deadline = sc_tick_now() + SC_TICK_FROM_SEC(2);
    while (panel->settings_busy) {
        assert(sc_tick_now() < deadline);
        assert(sc_control_panel_handle_event(panel, &tick));
        SDL_Delay(1);
    }
}

static void
test_quick_settings(void) {
    struct sc_control_panel panel;
    struct recorder rec;
    struct sc_device_settings settings;
    device_settings_fake_reset();
    setup(&panel, &rec);
    assert(sc_device_settings_init(&settings, "board-A"));
    sc_control_panel_set_device_settings(&panel, &settings);
    assert(panel.settings_busy);
    click(&panel, 205, 617); // ignore another action while initial read is pending
    wait_settings(&panel);
    assert(device_fake.calls == 1);
    assert(panel.device_info.battery_percent == 72);
    assert(panel.device_info.timeout_ms == 2000);
    click(&panel, 95, 617); // 1 minute
    assert(panel.settings_busy);
    assert(panel.device_info.timeout_ms == 2000); // do not optimistically mark applied
    wait_settings(&panel);
    assert(panel.device_info.timeout_ms == 60000);
    assert(sc_device_info_matches(&panel.device_info, SC_DEVICE_ACTION_TIMEOUT_1));
    device_fake.clamp_timeout = true;
    click(&panel, 150, 617); // firmware refuses/clamps 3 minutes
    wait_settings(&panel);
    assert(panel.settings_result == SC_DEVICE_RESULT_SET_FAILED);
    assert(!sc_device_info_matches(&panel.device_info, SC_DEVICE_ACTION_TIMEOUT_3));
    device_fake.clamp_timeout = false;
    click(&panel, 50, 588); // refresh
    wait_settings(&panel);
    assert(panel.settings_result == SC_DEVICE_RESULT_OK);
    assert(!rec.count); // these controls must never inject Android key events
    assert(!panel.settings_timer);
    sc_control_panel_disconnect(&panel);
    assert(!panel.settings && !panel.settings_busy);
    sc_device_settings_destroy(&settings);
}

static void
test_render(const char *preview) {
    struct sc_control_panel panel;
    struct recorder rec;
    setup(&panel, &rec);
    panel.device_info = (struct sc_device_info) {
        .fields = SC_DEVICE_INFO_BATTERY | SC_DEVICE_INFO_STORAGE
                | SC_DEVICE_INFO_MEMORY_TOTAL | SC_DEVICE_INFO_MEMORY_AVAILABLE
                | SC_DEVICE_INFO_TIMEOUT | SC_DEVICE_INFO_STAY_AWAKE,
        .battery_percent = 72, .storage_total_kib = 50086764,
        .storage_used_kib = 5458312, .memory_total_kib = 3871180,
        .memory_available_kib = 2771844, .timeout_ms = 300000,
    };
    SDL_Surface *surface = SDL_CreateSurface(1280, 640, SDL_PIXELFORMAT_RGBA32);
    assert(surface);
    SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface);
    assert(renderer);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    assert(SDL_RenderClear(renderer));
    sc_control_panel_render(&panel, renderer, 1);
    assert(SDL_RenderPresent(renderer));
    if (preview) {
        assert(SDL_SaveBMP(surface, preview));
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
    sc_control_panel_release_all(&panel);
}

int
main(int argc, char *argv[]) {
    assert(SDL_Init(SDL_INIT_EVENTS));
    test_press_drag_release();
    test_modifiers_and_focus();
    test_lock_repeat_and_disconnect();
    test_scaling_touch_and_disabled();
    test_failed_enqueue();
    test_quick_settings();
    test_render(argc > 1 ? argv[1] : NULL);
    SDL_Quit();
    return 0;
}
