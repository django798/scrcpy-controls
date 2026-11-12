/* Copyright (C) 2026 moasm */
/* SPDX-License-Identifier: Apache-2.0 */
#include "adb.h"

#include <assert.h>

#include "util/process.h"

bool
sc_adb_shell_capture(struct sc_intr *intr, const char *serial,
                      const char *script, char *output, size_t capacity) {
    assert(capacity > 1);
    output[0] = '\0';
    if (sc_intr_is_interrupted(intr)) {
        return false;
    }
    const char *const argv[] = {sc_adb_get_executable(), "-s", serial,
                                "shell", script, NULL};
    sc_pipe pipe;
    sc_pid pid;
    enum sc_process_result spawned = sc_process_execute_p(argv, &pid,
        SC_PROCESS_NO_STDERR, NULL, &pipe, NULL);
    if (spawned != SC_PROCESS_SUCCESS) {
        return false;
    }
    if (!sc_intr_set_process(intr, pid)) {
        sc_process_terminate(pid);
        sc_process_wait(pid, true);
        sc_pipe_close(pipe);
        return false;
    }
    // Register the process across both read and wait. Stop/timeout can kill it,
    // but the PID/handle stays valid until it has been unregistered below.
    ssize_t size = sc_pipe_read_all(pipe, output, capacity - 1);
    sc_pipe_close(pipe);
    bool complete = size >= 0 && (size_t) size < capacity - 1;
    if (!complete) {
        sc_process_terminate(pid);
    }
    sc_exit_code code = sc_process_wait(pid, false);
    sc_intr_set_process(intr, SC_PROCESS_NONE);
    sc_process_close(pid);
    if (!complete || code || sc_intr_is_interrupted(intr)) {
        output[0] = '\0';
        return false;
    }
    output[size] = '\0';
    return true;
}
