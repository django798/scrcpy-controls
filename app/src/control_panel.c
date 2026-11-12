/* Copyright (C) 2026 moasm */
/* SPDX-License-Identifier: Apache-2.0 */
#include "control_panel.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "events.h"
#include "util/log.h"

#define PANEL_SIZE 640.f
#define UNIT (608.f / 15)
#define REPEAT_DELAY 500
#define REPEAT_INTERVAL 50

struct panel_key {
    SDL_FRect rect; // design coordinates in a 640 x 640 panel
    const char *label;
    const char *shifted;
    enum android_keycode code;
    uint32_t modifier;
    uint32_t lock;
    enum sc_device_action action;
};

#define KEY(X,Y,W,H,L,S,C,M,K) \
    {{X,Y,W,H}, L, S, AKEYCODE_##C, M, K, SC_DEVICE_ACTION_NONE}
#define SETTING(X,Y,W,L,A) \
    {{X,Y,W,24}, L, NULL, AKEYCODE_UNKNOWN, 0, 0, SC_DEVICE_ACTION_##A}
#define K(R,X,W,L,S,C) \
    KEY(16+(X)*UNIT,390+(R)*36,(W)*UNIT-4,32,L,S,C,0,0)
#define MOD(R,X,W,L,C,M) \
    KEY(16+(X)*UNIT,390+(R)*36,(W)*UNIT-4,32,L,NULL,C,AMETA_##M,0)
#define FN(X,L,C) KEY(16+(X)*38,354,34,28,L,NULL,C,0,0)
#define NUM(X,Y,W,H,L,C) \
    KEY(396+(X)*53,192+(Y)*30,(W)*53-4,(H)*30-4,L,NULL,C,0,0)

static const struct panel_key keys[] = {
    KEY(16,66,96,42,"Power",NULL,POWER,0,0),
    KEY(118,66,96,42,"Back",NULL,BACK,0,0),
    KEY(220,66,96,42,"Home",NULL,HOME,0,0),
    KEY(322,66,96,42,"Recents",NULL,APP_SWITCH,0,0),
    KEY(424,66,96,42,"Vol -",NULL,VOLUME_DOWN,0,0),
    KEY(526,66,98,42,"Vol +",NULL,VOLUME_UP,0,0),
    KEY(16,116,146,32,"Menu",NULL,MENU,0,0),
    KEY(170,116,146,32,"Search",NULL,SEARCH,0,0),
    KEY(324,116,146,32,"Wake",NULL,WAKEUP,0,0),
    KEY(478,116,146,32,"Sleep",NULL,SLEEP,0,0),

    KEY(70,200,50,34,"Up",NULL,DPAD_UP,0,0),
    KEY(16,238,50,34,"Left",NULL,DPAD_LEFT,0,0),
    KEY(70,238,50,34,"OK",NULL,DPAD_CENTER,0,0),
    KEY(124,238,50,34,"Right",NULL,DPAD_RIGHT,0,0),
    KEY(70,276,50,34,"Down",NULL,DPAD_DOWN,0,0),
    KEY(190,200,86,32,"Home",NULL,MOVE_HOME,0,0),
    KEY(282,200,86,32,"End",NULL,MOVE_END,0,0),
    KEY(190,238,86,32,"PgUp",NULL,PAGE_UP,0,0),
    KEY(282,238,86,32,"PgDn",NULL,PAGE_DOWN,0,0),
    KEY(190,276,86,32,"Insert",NULL,INSERT,0,0),
    KEY(282,276,86,32,"Delete",NULL,FORWARD_DEL,0,0),

    KEY(396,192,49,26,"Num",NULL,NUM_LOCK,0,AMETA_NUM_LOCK_ON),
    NUM(1,0,1,1,"/",NUMPAD_DIVIDE),
    NUM(2,0,1,1,"*",NUMPAD_MULTIPLY),
    NUM(3,0,1,1,"-",NUMPAD_SUBTRACT),
    NUM(0,1,1,1,"7",NUMPAD_7), NUM(1,1,1,1,"8",NUMPAD_8),
    NUM(2,1,1,1,"9",NUMPAD_9), NUM(3,1,1,2,"+",NUMPAD_ADD),
    NUM(0,2,1,1,"4",NUMPAD_4), NUM(1,2,1,1,"5",NUMPAD_5),
    NUM(2,2,1,1,"6",NUMPAD_6),
    NUM(0,3,1,1,"1",NUMPAD_1), NUM(1,3,1,1,"2",NUMPAD_2),
    NUM(2,3,1,1,"3",NUMPAD_3), NUM(3,3,1,2,"Ent",NUMPAD_ENTER),
    NUM(0,4,2,1,"0",NUMPAD_0), NUM(2,4,1,1,".",NUMPAD_DOT),

    FN(0,"Esc",ESCAPE), FN(1,"F1",F1), FN(2,"F2",F2),
    FN(3,"F3",F3), FN(4,"F4",F4), FN(5,"F5",F5),
    FN(6,"F6",F6), FN(7,"F7",F7), FN(8,"F8",F8),
    FN(9,"F9",F9), FN(10,"F10",F10), FN(11,"F11",F11),
    FN(12,"F12",F12), FN(13,"Prt",SYSRQ),
    KEY(548,354,34,28,"Scr",NULL,SCROLL_LOCK,0,AMETA_SCROLL_LOCK_ON),
    FN(15,"Pau",BREAK),

    K(0,0,1,"`","~",GRAVE),
    K(0,1,1,"1","!",1), K(0,2,1,"2","@",2),
    K(0,3,1,"3","#",3), K(0,4,1,"4","$",4),
    K(0,5,1,"5","%",5), K(0,6,1,"6","^",6),
    K(0,7,1,"7","&",7), K(0,8,1,"8","*",8),
    K(0,9,1,"9","(",9), K(0,10,1,"0",")",0),
    K(0,11,1,"-","_",MINUS), K(0,12,1,"=","+",EQUALS),
    K(0,13,2,"Bksp",NULL,DEL),

    K(1,0,1.5,"Tab",NULL,TAB),
    K(1,1.5,1,"Q",NULL,Q), K(1,2.5,1,"W",NULL,W),
    K(1,3.5,1,"E",NULL,E), K(1,4.5,1,"R",NULL,R),
    K(1,5.5,1,"T",NULL,T), K(1,6.5,1,"Y",NULL,Y),
    K(1,7.5,1,"U",NULL,U), K(1,8.5,1,"I",NULL,I),
    K(1,9.5,1,"O",NULL,O), K(1,10.5,1,"P",NULL,P),
    K(1,11.5,1,"[","{",LEFT_BRACKET),
    K(1,12.5,1,"]","}",RIGHT_BRACKET),
    K(1,13.5,1.5,"\\","|",BACKSLASH),

    KEY(16,462,1.75*UNIT-4,32,"Caps",NULL,CAPS_LOCK,0,AMETA_CAPS_LOCK_ON),
    K(2,1.75,1,"A",NULL,A), K(2,2.75,1,"S",NULL,S),
    K(2,3.75,1,"D",NULL,D), K(2,4.75,1,"F",NULL,F),
    K(2,5.75,1,"G",NULL,G), K(2,6.75,1,"H",NULL,H),
    K(2,7.75,1,"J",NULL,J), K(2,8.75,1,"K",NULL,K),
    K(2,9.75,1,"L",NULL,L),
    K(2,10.75,1,";",":",SEMICOLON),
    K(2,11.75,1,"'","\"",APOSTROPHE),
    K(2,12.75,2.25,"Enter",NULL,ENTER),

    MOD(3,0,2.25,"Shift",SHIFT_LEFT,SHIFT_LEFT_ON),
    K(3,2.25,1,"Z",NULL,Z), K(3,3.25,1,"X",NULL,X),
    K(3,4.25,1,"C",NULL,C), K(3,5.25,1,"V",NULL,V),
    K(3,6.25,1,"B",NULL,B), K(3,7.25,1,"N",NULL,N),
    K(3,8.25,1,"M",NULL,M),
    K(3,9.25,1,",","<",COMMA), K(3,10.25,1,".",">",PERIOD),
    K(3,11.25,1,"/","?",SLASH),
    MOD(3,12.25,2.75,"Shift",SHIFT_RIGHT,SHIFT_RIGHT_ON),

    MOD(4,0,1.5,"Ctrl",CTRL_LEFT,CTRL_LEFT_ON),
    MOD(4,1.5,1.25,"Meta",META_LEFT,META_LEFT_ON),
    MOD(4,2.75,1.25,"Alt",ALT_LEFT,ALT_LEFT_ON),
    K(4,4,6,"Space",NULL,SPACE),
    MOD(4,10,1.25,"Alt",ALT_RIGHT,ALT_RIGHT_ON),
    MOD(4,11.25,1.25,"Meta",META_RIGHT,META_RIGHT_ON),
    K(4,12.5,1,"Menu",NULL,MENU),
    MOD(4,13.5,1.5,"Ctrl",CTRL_RIGHT,CTRL_RIGHT_ON),

    SETTING(16,576,68,"Info",READ),
    SETTING(72,606,48,"1m",TIMEOUT_1),
    SETTING(126,606,48,"3m",TIMEOUT_3),
    SETTING(180,606,48,"5m",TIMEOUT_5),
    SETTING(234,606,56,"30m",TIMEOUT_30),
    SETTING(296,606,76,"Always",ALWAYS),
};

static uint32_t
metastate(const struct sc_control_panel *panel) {
    uint32_t mods = panel->modifiers;
    if (mods & (AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_RIGHT_ON)) {
        mods |= AMETA_SHIFT_ON;
    }
    if (mods & (AMETA_CTRL_LEFT_ON | AMETA_CTRL_RIGHT_ON)) {
        mods |= AMETA_CTRL_ON;
    }
    if (mods & (AMETA_ALT_LEFT_ON | AMETA_ALT_RIGHT_ON)) {
        mods |= AMETA_ALT_ON;
    }
    if (mods & (AMETA_META_LEFT_ON | AMETA_META_RIGHT_ON)) {
        mods |= AMETA_META_ON;
    }
    return mods | panel->locks;
}

static bool
send_key(struct sc_control_panel *panel, int index,
         enum android_keyevent_action action, uint32_t repeat) {
    if (!panel->connected) {
        return false;
    }
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_INJECT_KEYCODE,
        .inject_keycode = {
            .action = action,
            .keycode = keys[index].code,
            .repeat = repeat,
            .metastate = metastate(panel),
        },
    };
    bool ok = panel->send(panel->userdata, &msg);
    if (!ok) {
        LOGW("Could not enqueue panel key: %s", keys[index].label);
    }
    return ok;
}

void
sc_control_panel_init(struct sc_control_panel *panel, bool enabled,
                      sc_panel_send_fn send, void *userdata) {
    *panel = (struct sc_control_panel) {
        .enabled = enabled,
        .connected = enabled,
        .repeat_enabled = true,
        .hovered = -1,
        .pressed = -1,
        .send = send,
        .userdata = userdata,
    };
    assert(!enabled || send);
}

uint16_t
sc_control_panel_width(struct sc_size size) {
    return MIN(640, MIN(size.width * 3 / 5, size.height));
}

void
sc_control_panel_layout(struct sc_control_panel *panel,
                        struct sc_size size) {
    float width = panel->enabled ? sc_control_panel_width(size) : 0;
    panel->rect = (SDL_FRect) {size.width - width, 0, width, size.height};
    panel->hovered = -1;
}

static void
draw_text(SDL_Renderer *renderer, float x, float y, const char *text,
          float scale, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_SetRenderScale(renderer, scale, scale);
    SDL_RenderDebugText(renderer, x / scale, y / scale, text);
    SDL_SetRenderScale(renderer, 1, 1);
}

void
sc_control_panel_render(const struct sc_control_panel *panel,
                        SDL_Renderer *renderer, float density) {
    if (!panel->enabled || !panel->rect.w) {
        return;
    }
    float old_x, old_y;
    SDL_GetRenderScale(renderer, &old_x, &old_y);
    SDL_SetRenderScale(renderer, 1, 1);

    float scale = panel->rect.w / PANEL_SIZE * density;
    float ox = panel->rect.x * density;
    float oy = (panel->rect.h - panel->rect.w) / 2 * density;
    SDL_FRect bg = {ox, 0, panel->rect.w * density, panel->rect.h * density};
    SDL_SetRenderDrawColor(renderer, 22, 28, 38, 255);
    SDL_RenderFillRect(renderer, &bg);
    SDL_SetRenderDrawColor(renderer, 52, 64, 80, 255);
    SDL_RenderLine(renderer, ox, 0, ox, bg.h);

    SDL_Color text = {225, 232, 242, 255};
    SDL_Color muted = {146, 163, 184, 255};
    SDL_Color accent = {100, 218, 185, 255};
    draw_text(renderer, ox + 16*scale, oy + 18*scale,
              "DEVICE CONTROLS", scale * 1.5f, text);
    draw_text(renderer, ox + 486*scale, oy + 22*scale,
              panel->connected ? "CONNECTED" : "DISCONNECTED", scale,
              panel->connected ? accent : muted);
    draw_text(renderer, ox + 16*scale, oy + 42*scale,
              "Android keys  /  click or hold", scale, muted);
    draw_text(renderer, ox + 16*scale, oy + 174*scale,
              "DIRECTION", scale, muted);
    draw_text(renderer, ox + 190*scale, oy + 174*scale,
              "EDIT", scale, muted);
    draw_text(renderer, ox + 396*scale, oy + 174*scale,
              "NUMBER PAD", scale, muted);

    uint32_t meta = metastate(panel);
    for (size_t i = 0; i < ARRAY_LEN(keys); ++i) {
        const struct panel_key *key = &keys[i];
        bool active = (key->modifier && (panel->modifiers & key->modifier))
                   || (key->lock && (panel->locks & key->lock))
                   || sc_device_info_matches(&panel->device_info, key->action);
        bool available = panel->connected && (key->action == SC_DEVICE_ACTION_NONE
                         || (panel->settings && !panel->settings_busy));
        bool pressed = (int) i == panel->pressed;
        SDL_FRect r = {ox + key->rect.x * scale, oy + key->rect.y * scale,
                       key->rect.w * scale, key->rect.h * scale};
        if (active || pressed) {
            SDL_SetRenderDrawColor(renderer, 35, 110, 109, 255);
        } else if ((int) i == panel->hovered && available) {
            SDL_SetRenderDrawColor(renderer, 65, 79, 100, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 39, 49, 65, 255);
        }
        SDL_RenderFillRect(renderer, &r);
        SDL_SetRenderDrawColor(renderer, active ? 100 : 72,
                               active ? 218 : 88, active ? 185 : 108, 255);
        SDL_RenderRect(renderer, &r);
        const char *label = (meta & AMETA_SHIFT_ON) && key->shifted
                          ? key->shifted : key->label;
        float text_scale = scale * MIN(1.25f,
                                      (key->rect.w - 6) / (8 * strlen(label)));
        float tw = strlen(label) * 8 * text_scale;
        draw_text(renderer, r.x + (r.w - tw) / 2,
                  r.y + (r.h - 8*text_scale) / 2, label, text_scale,
                  available ? text : muted);
    }
    char info[192];
    sc_device_info_format(&panel->device_info, info, sizeof(info));
    float info_scale = scale * MIN(1.f, 532.f / (8 * strlen(info)));
    draw_text(renderer, ox + 92*scale, oy + 584*scale, info, info_scale, text);
    draw_text(renderer, ox + 16*scale, oy + 614*scale, "Sleep", scale, muted);
    const char *status;
    SDL_Color status_color = muted;
    if (!panel->settings || !panel->connected) {
        status = "Unavailable";
    } else if (panel->settings_busy) {
        status = panel->settings_action == SC_DEVICE_ACTION_READ
               ? "Reading..." : "Setting...";
    } else if (panel->settings_result != SC_DEVICE_RESULT_OK) {
        status = panel->settings_result == SC_DEVICE_RESULT_TIMEOUT ? "Timed out"
               : panel->settings_result == SC_DEVICE_RESULT_SET_FAILED ? "Not applied"
               : "Read failed";
        status_color = (SDL_Color) {242, 161, 130, 255};
    } else if ((panel->device_info.fields & SC_DEVICE_INFO_STAY_AWAKE)
            && panel->device_info.stay_awake
            && !sc_device_info_matches(&panel->device_info, SC_DEVICE_ACTION_ALWAYS)) {
        status = "Plugged awake";
    } else {
        status = panel->settings_action == SC_DEVICE_ACTION_READ ? "Read" : "Applied";
        status_color = accent;
    }
    // Keep the measurement basis visible during reads, writes and errors.
    // This legend applies to both Data and RAM in the row above.
    char status_text[48];
    snprintf(status_text, sizeof(status_text), "Used/Total GiB | %s", status);
    float status_scale = scale * MIN(1.f, 240.f / (8 * strlen(status_text)));
    draw_text(renderer, ox + 384*scale, oy + 614*scale, status_text,
              status_scale, status_color);
    SDL_SetRenderScale(renderer, old_x, old_y);
}

static int
hit_test(const struct sc_control_panel *panel, float x, float y) {
    float scale = panel->rect.w / PANEL_SIZE;
    if (!scale) {
        return -1;
    }
    x = (x - panel->rect.x) / scale;
    y = (y - (panel->rect.h - panel->rect.w) / 2) / scale;
    for (size_t i = 0; i < ARRAY_LEN(keys); ++i) {
        if (SDL_PointInRectFloat(&(SDL_FPoint) {x, y}, &keys[i].rect)) {
            return i;
        }
    }
    return -1;
}

static Uint32 SDLCALL
repeat_timer(void *userdata, SDL_TimerID timer, Uint32 interval) {
    (void) userdata;
    (void) timer;
    SDL_Event event = {.type = SC_EVENT_CONTROL_PANEL_TICK};
    SDL_PushEvent(&event);
    return interval;
}

static void
release_key(struct sc_control_panel *panel) {
    if (panel->timer) {
        SDL_RemoveTimer(panel->timer);
        panel->timer = 0;
    }
    if (panel->pressed >= 0) {
        send_key(panel, panel->pressed, AKEY_EVENT_ACTION_UP, 0);
        panel->pressed = -1;
    }
}

void
sc_control_panel_release_all(struct sc_control_panel *panel) {
    release_key(panel);
    for (size_t i = 0; i < ARRAY_LEN(keys); ++i) {
        if (keys[i].modifier & panel->modifiers) {
            panel->modifiers &= ~keys[i].modifier;
            send_key(panel, i, AKEY_EVENT_ACTION_UP, 0);
        }
    }
    panel->captured_buttons = 0;
    panel->finger_down = false;
    panel->hovered = -1;
}

void
sc_control_panel_disconnect(struct sc_control_panel *panel) {
    // The controller may already have stopped; only clear local state.
    panel->connected = false;
    sc_control_panel_release_all(panel);
    if (panel->settings_timer) {
        SDL_RemoveTimer(panel->settings_timer);
        panel->settings_timer = 0;
    }
    if (panel->settings) {
        sc_device_settings_stop(panel->settings);
        panel->settings = NULL;
    }
    panel->settings_busy = false;
}

static Uint32 SDLCALL
settings_timer(void *userdata, SDL_TimerID timer, Uint32 interval) {
    (void) userdata;
    (void) timer;
    SDL_Event event = {.type = SC_EVENT_DEVICE_SETTINGS_TICK};
    SDL_PushEvent(&event);
    return interval;
}

static void
request_settings(struct sc_control_panel *panel, enum sc_device_action action) {
    if (!panel->settings || panel->settings_busy) {
        return;
    }
    panel->settings_action = action;
    ++panel->settings_revision;
    if (!sc_device_settings_request(panel->settings, action)) {
        panel->settings_result = action == SC_DEVICE_ACTION_READ
                               ? SC_DEVICE_RESULT_READ_FAILED : SC_DEVICE_RESULT_SET_FAILED;
        return;
    }
    panel->settings_busy = true;
    panel->settings_timer = SDL_AddTimer(100, settings_timer, NULL);
    if (!panel->settings_timer) {
        LOGW("Could not start device settings timer: %s", SDL_GetError());
        sc_device_settings_stop(panel->settings);
        panel->settings_busy = false;
        panel->settings_result = SC_DEVICE_RESULT_READ_FAILED;
    }
}

void
sc_control_panel_set_device_settings(struct sc_control_panel *panel,
                                      struct sc_device_settings *settings) {
    assert(!panel->settings);
    panel->settings = settings;
    if (panel->enabled && panel->connected) {
        request_settings(panel, SC_DEVICE_ACTION_READ);
    }
}

static void
press_key(struct sc_control_panel *panel, float x, float y) {
    int index = hit_test(panel, x, y);
    if (index < 0 || !panel->connected) {
        return;
    }
    release_key(panel);
    const struct panel_key *key = &keys[index];
    if (key->action != SC_DEVICE_ACTION_NONE) {
        request_settings(panel, key->action);
    } else if (key->modifier) {
        panel->modifiers ^= key->modifier;
        bool down = panel->modifiers & key->modifier;
        if (!send_key(panel, index, down ? AKEY_EVENT_ACTION_DOWN
                                        : AKEY_EVENT_ACTION_UP, 0)) {
            panel->modifiers ^= key->modifier;
        }
    } else if (key->lock) {
        panel->locks ^= key->lock;
        if (send_key(panel, index, AKEY_EVENT_ACTION_DOWN, 0)) {
            send_key(panel, index, AKEY_EVENT_ACTION_UP, 0);
        } else {
            panel->locks ^= key->lock;
        }
    } else if (send_key(panel, index, AKEY_EVENT_ACTION_DOWN, 0)) {
        panel->pressed = index;
        panel->repeat = 0;
        panel->next_repeat = SDL_GetTicks() + REPEAT_DELAY;
        // System keys implement hold handling in Android policy. Repeated
        // POWER/HOME/BACK events could toggle or navigate more than once.
        bool repeatable = key->code != AKEYCODE_POWER
                       && key->code != AKEYCODE_HOME
                       && key->code != AKEYCODE_BACK
                       && key->code != AKEYCODE_APP_SWITCH
                       && key->code != AKEYCODE_SLEEP
                       && key->code != AKEYCODE_WAKEUP;
        if (repeatable && panel->repeat_enabled) {
            panel->timer = SDL_AddTimer(REPEAT_INTERVAL, repeat_timer, NULL);
            if (!panel->timer) {
                LOGW("Could not start panel key repeat: %s", SDL_GetError());
            }
        }
    }
}

bool
sc_control_panel_handle_event(struct sc_control_panel *panel,
                              const SDL_Event *event) {
    if (!panel->enabled) {
        return false;
    }
    switch (event->type) {
        case SC_EVENT_DEVICE_SETTINGS_TICK:
            if (panel->settings_busy && panel->settings) {
                struct sc_device_info info;
                enum sc_device_result result;
                if (sc_device_settings_poll(panel->settings, &info, &result)) {
                    if (info.fields) {
                        panel->device_info = info;
                    }
                    panel->settings_result = result;
                    panel->settings_busy = false;
                    SDL_RemoveTimer(panel->settings_timer);
                    panel->settings_timer = 0;
                    ++panel->settings_revision;
                }
            }
            return true;
        case SC_EVENT_CONTROL_PANEL_TICK:
            if (panel->pressed >= 0 && panel->timer
                    && SDL_GetTicks() >= panel->next_repeat) {
                send_key(panel, panel->pressed, AKEY_EVENT_ACTION_DOWN,
                         ++panel->repeat);
                panel->next_repeat = SDL_GetTicks() + REPEAT_INTERVAL;
            }
            return true;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
        case SDL_EVENT_WINDOW_HIDDEN:
        case SDL_EVENT_WINDOW_MINIMIZED:
            sc_control_panel_release_all(panel);
            return false; // other window handlers may also need the event
        case SDL_EVENT_MOUSE_MOTION: {
            bool inside = SDL_PointInRectFloat(
                &(SDL_FPoint) {event->motion.x, event->motion.y}, &panel->rect);
            panel->hovered = inside
                           ? hit_test(panel, event->motion.x, event->motion.y)
                           : -1;
            return panel->captured_buttons || (inside && !event->motion.state);
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            if (panel->finger_down || !SDL_PointInRectFloat(
                    &(SDL_FPoint) {event->button.x, event->button.y},
                    &panel->rect)) {
                return false;
            }
            panel->captured_buttons |= SDL_BUTTON_MASK(event->button.button);
            if (event->button.button == SDL_BUTTON_LEFT) {
                press_key(panel, event->button.x, event->button.y);
            }
            return true;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            SDL_MouseButtonFlags button = SDL_BUTTON_MASK(event->button.button);
            if (!(panel->captured_buttons & button)) {
                return false;
            }
            panel->captured_buttons &= ~button;
            if (event->button.button == SDL_BUTTON_LEFT) {
                release_key(panel);
            }
            return true;
        }
        case SDL_EVENT_MOUSE_WHEEL:
            return SDL_PointInRectFloat(
                &(SDL_FPoint) {event->wheel.mouse_x, event->wheel.mouse_y},
                &panel->rect);
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_CANCELED: {
            float x = event->tfinger.x * (panel->rect.x + panel->rect.w);
            float y = event->tfinger.y * panel->rect.h;
            bool down = event->type == SDL_EVENT_FINGER_DOWN;
            if (down) {
                if (panel->finger_down || panel->captured_buttons
                        || !SDL_PointInRectFloat(&(SDL_FPoint) {x, y},
                                                 &panel->rect)) {
                    return false;
                }
                panel->finger = event->tfinger.fingerID;
                panel->finger_down = true;
                press_key(panel, x, y);
            } else if (!panel->finger_down
                    || event->tfinger.fingerID != panel->finger) {
                return false;
            } else if (event->type != SDL_EVENT_FINGER_MOTION) {
                release_key(panel);
                panel->finger_down = false;
            }
            return true;
        }
        default:
            return false;
    }
}
