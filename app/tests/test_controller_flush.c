/* Copyright (C) 2026 moasm */
/* SPDX-License-Identifier: Apache-2.0 */
#include "common.h"

#include <assert.h>
#include <string.h>
#include <SDL3/SDL.h>

#include "controller.h"

// A gated transport tests the actual controller worker and serializer without
// requiring a device. In particular, an empty queue can still have an in-flight
// key release, which must prevent flush from succeeding early.
static struct {
    sc_mutex mutex;
    sc_cond cond;
    bool entered;
    bool allow_write;
    bool fail_write;
    uint8_t bytes[64];
    size_t len;
} transport;

ssize_t
net_send_all(sc_socket socket, const void *buf, size_t len) {
    (void) socket;
    sc_mutex_lock(&transport.mutex);
    transport.entered = true;
    sc_cond_signal(&transport.cond);
    while (!transport.allow_write) {
        sc_cond_wait(&transport.cond, &transport.mutex);
    }
    bool failed = transport.fail_write;
    if (!failed) {
        assert(transport.len + len <= sizeof(transport.bytes));
        memcpy(transport.bytes + transport.len, buf, len);
        transport.len += len;
    }
    sc_mutex_unlock(&transport.mutex);
    return failed ? -1 : (ssize_t) len;
}

// These receiver stubs only isolate the unrelated Android-to-client channel.
bool
sc_receiver_init(struct sc_receiver *receiver, sc_socket socket,
                  const struct sc_receiver_callbacks *cbs, void *userdata) {
    (void) receiver; (void) socket; (void) cbs; (void) userdata;
    return true;
}

bool
sc_receiver_start(struct sc_receiver *receiver) {
    (void) receiver;
    return true;
}

void
sc_receiver_join(struct sc_receiver *receiver) {
    (void) receiver;
}

void
sc_receiver_destroy(struct sc_receiver *receiver) {
    (void) receiver;
}

static void
on_ended(struct sc_controller *controller, bool error, void *userdata) {
    (void) controller; (void) error; (void) userdata;
}

static void
test_flush(bool fail_write) {
    memset(&transport, 0, sizeof(transport));
    assert(sc_mutex_init(&transport.mutex));
    assert(sc_cond_init(&transport.cond));
    transport.fail_write = fail_write;
    struct sc_controller controller;
    static const struct sc_controller_callbacks callbacks = {.on_ended = on_ended};
    assert(sc_controller_init(&controller, SC_SOCKET_NONE, &callbacks, NULL));
    assert(sc_controller_start(&controller));
    assert(sc_controller_flush(&controller, sc_tick_now()));
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_INJECT_KEYCODE,
        .inject_keycode = {
            .keycode = AKEYCODE_SHIFT_LEFT,
            .action = AKEY_EVENT_ACTION_DOWN,
        },
    };
    assert(sc_controller_push_msg(&controller, &msg));
    sc_mutex_lock(&transport.mutex);
    sc_tick deadline = sc_tick_now() + SC_TICK_FROM_SEC(2);
    while (!transport.entered) {
        assert(sc_cond_timedwait(&transport.cond, &transport.mutex, deadline));
    }
    sc_mutex_unlock(&transport.mutex);
    // The worker has dequeued DOWN but the write has not completed.
    assert(!sc_controller_flush(&controller,
                               sc_tick_now() + SC_TICK_FROM_MS(20)));
    msg.inject_keycode.action = AKEY_EVENT_ACTION_UP;
    assert(sc_controller_push_msg(&controller, &msg));
    sc_mutex_lock(&transport.mutex);
    transport.allow_write = true;
    sc_cond_signal(&transport.cond);
    sc_mutex_unlock(&transport.mutex);
    bool flushed = sc_controller_flush(&controller,
                                       sc_tick_now() + SC_TICK_FROM_SEC(2));
    assert(flushed == !fail_write);
    sc_controller_stop(&controller);
    sc_controller_join(&controller);
    if (!fail_write) {
        assert(transport.len == 28);
        assert(transport.bytes[0] == SC_CONTROL_MSG_TYPE_INJECT_KEYCODE);
        assert(transport.bytes[1] == AKEY_EVENT_ACTION_DOWN);
        assert(transport.bytes[5] == AKEYCODE_SHIFT_LEFT);
        assert(transport.bytes[14] == SC_CONTROL_MSG_TYPE_INJECT_KEYCODE);
        assert(transport.bytes[15] == AKEY_EVENT_ACTION_UP);
        assert(transport.bytes[19] == AKEYCODE_SHIFT_LEFT);
    }
    sc_controller_destroy(&controller);
    sc_cond_destroy(&transport.cond);
    sc_mutex_destroy(&transport.mutex);
}

int
main(void) {
    assert(SDL_Init(SDL_INIT_EVENTS));
    test_flush(false);
    test_flush(true);
    SDL_Quit();
    return 0;
}
