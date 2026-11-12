/* Copyright (C) 2026 moasm */
/* SPDX-License-Identifier: Apache-2.0 */
#ifndef SC_CONTROL_PANEL_H
#define SC_CONTROL_PANEL_H

#include "common.h"

#include <SDL3/SDL.h>
#include "control_msg.h"
#include "coords.h"
#include "device_settings.h"

typedef bool (*sc_panel_send_fn)(void *userdata,
                                 const struct sc_control_msg *msg);

struct sc_control_panel {
    bool enabled;
    bool connected;
    bool repeat_enabled;
    SDL_FRect rect; // window coordinates, independent of pixel density
    int hovered;
    int pressed;
    SDL_MouseButtonFlags captured_buttons;
    SDL_FingerID finger;
    bool finger_down;
    uint32_t modifiers; // directional modifier bits only
    uint32_t locks;
    uint32_t repeat;
    uint64_t next_repeat;
    SDL_TimerID timer;
    sc_panel_send_fn send;
    void *userdata;
    struct sc_device_settings *settings; // owned by the screen
    struct sc_device_info device_info;
    enum sc_device_action settings_action;
    enum sc_device_result settings_result;
    bool settings_busy;
    SDL_TimerID settings_timer;
    unsigned settings_revision;
};

void
sc_control_panel_init(struct sc_control_panel *panel, bool enabled,
                      sc_panel_send_fn send, void *userdata);

// Attach the per-device background service and read an initial snapshot.
void
sc_control_panel_set_device_settings(struct sc_control_panel *panel,
                                      struct sc_device_settings *settings);

// The panel occupies at most 60% of a narrow window, leaving video accessible.
uint16_t
sc_control_panel_width(struct sc_size window_size);

void
sc_control_panel_layout(struct sc_control_panel *panel,
                        struct sc_size window_size);

void
sc_control_panel_render(const struct sc_control_panel *panel,
                        SDL_Renderer *renderer, float pixel_density);

// Returns true only for events consumed by the panel. In particular, a mouse
// release from a drag that started on the video must still reach the video.
bool
sc_control_panel_handle_event(struct sc_control_panel *panel,
                              const SDL_Event *event);

// Call before stopping the controller, and on focus loss. Locks are retained,
// while all held keys and latched modifiers are released.
void
sc_control_panel_release_all(struct sc_control_panel *panel);

void
sc_control_panel_disconnect(struct sc_control_panel *panel);

#endif
