/* Copyright (C) 2026 moasm */
/* SPDX-License-Identifier: Apache-2.0 */
#include "multi_device.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "adb/adb.h"
#include "util/file.h"
#include "util/log.h"
#include "util/process.h"

static bool
should_check_devices(const struct scrcpy_options *opts) {
    const char *env_serial = getenv("ANDROID_SERIAL");
    if (opts->serial || opts->select_usb || opts->select_tcpip || opts->tcpip_dst
            || (env_serial && *env_serial) || opts->list || !opts->window
            || !opts->video_playback) {
        return false;
    }
#ifdef HAVE_USB
    if (opts->otg) {
        return false;
    }
#endif
    return true;
}

static char *
make_window_title(const struct sc_adb_device *device) {
    const char *model = device->model && *device->model ? device->model : "Android";
    size_t len = strlen(model) + strlen(device->serial) + sizeof(" []");
    char *title = malloc(len);
    if (title) {
        snprintf(title, len, "%s [%s]", model, device->serial);
    } else {
        LOG_OOM();
    }
    return title;
}

static enum scrcpy_exit_code
launch_devices(int argc, char *const argv[], const struct scrcpy_options *opts,
                const struct sc_vec_adb_devices *devices) {
    char *executable = sc_file_get_executable_path();
    if (!executable) {
        LOGE("Could not resolve the scrcpy executable for multiple devices");
        return SCRCPY_EXIT_FAILURE;
    }
    sc_pid *processes = calloc(devices->size, sizeof(*processes));
    const char **child_argv = malloc(((size_t) argc + 6) * sizeof(*child_argv));
    if (!processes || !child_argv) {
        LOG_OOM();
        free(processes);
        free(child_argv);
        free(executable);
        return SCRCPY_EXIT_FAILURE;
    }

    enum scrcpy_exit_code result = SCRCPY_EXIT_SUCCESS;
    for (size_t i = 0; i < devices->size; ++i) {
        processes[i] = SC_PROCESS_NONE;
        const struct sc_adb_device *device = &devices->data[i];
        if (strcmp(device->state, "device")) {
            continue;
        }

        char *title = NULL;
        size_t count = 0;
        child_argv[count++] = executable;
        child_argv[count++] = "--serial";
        child_argv[count++] = device->serial; // also prevents recursive fan-out
        child_argv[count++] = "--no-terminal-title";
        if (!opts->window_title) {
            title = make_window_title(device);
            if (!title) {
                result = SCRCPY_EXIT_FAILURE;
                continue;
            }
            child_argv[count++] = "--window-title";
            child_argv[count++] = title;
        }
        // Prefix our options so a user's trailing '--' remains valid. An
        // explicitly supplied window title and all other arguments are kept.
        for (int j = 1; j < argc; ++j) {
            child_argv[count++] = argv[j];
        }
        child_argv[count] = NULL;

        LOGI("Opening device %s (%s)", device->serial,
             device->model ? device->model : "Android");
        enum sc_process_result spawned =
            sc_process_execute(child_argv, &processes[i], 0);
        free(title);
        if (spawned != SC_PROCESS_SUCCESS) {
            processes[i] = SC_PROCESS_NONE;
            LOGE("Could not start scrcpy for device %s", device->serial);
            result = SCRCPY_EXIT_FAILURE;
        }
    }

    // A failed/disconnected device does not terminate any other device's
    // window. Reap every process so Windows handles/Unix children are cleaned up.
    for (size_t i = 0; i < devices->size; ++i) {
        if (processes[i] == SC_PROCESS_NONE) {
            continue;
        }
        sc_exit_code code = sc_process_wait(processes[i], true);
        if (code != SCRCPY_EXIT_SUCCESS) {
            LOGW("Device %s exited with code %" SC_PRIexitcode,
                 devices->data[i].serial, code);
            if (code != SCRCPY_EXIT_DISCONNECTED) {
                result = SCRCPY_EXIT_FAILURE;
            } else if (result == SCRCPY_EXIT_SUCCESS) {
                result = SCRCPY_EXIT_DISCONNECTED;
            }
        }
    }
    free(processes);
    free(child_argv);
    free(executable);
    return result;
}

bool
sc_multi_device_run(int argc, char *const argv[],
                     const struct scrcpy_options *opts,
                     enum scrcpy_exit_code *exit_code) {
    assert(argc >= 1);
    if (!should_check_devices(opts)) {
        return false;
    }
    *exit_code = SCRCPY_EXIT_FAILURE;
    if (!sc_adb_init()) {
        return true;
    }
    struct sc_intr intr;
    if (!sc_intr_init(&intr)) {
        sc_adb_destroy();
        return true;
    }
    struct sc_vec_adb_devices devices = SC_VECTOR_INITIALIZER;
    bool listed = sc_adb_list_devices(&intr, 0, &devices);
    sc_intr_destroy(&intr);
    sc_adb_destroy();
    if (!listed) {
        sc_adb_devices_destroy(&devices);
        return true;
    }
    // Preserve the original startup and error reporting for zero/one transport.
    if (devices.size <= 1) {
        sc_adb_devices_destroy(&devices);
        return false;
    }

    size_t online_count = 0;
    for (size_t i = 0; i < devices.size; ++i) {
        const struct sc_adb_device *device = &devices.data[i];
        if (!strcmp(device->state, "device")) {
            ++online_count;
        } else {
            LOGW("Skipping device %s (state=%s)", device->serial, device->state);
        }
    }
    if (!online_count) {
        LOGE("No online authorized ADB devices; connect/unlock and authorize a device");
    } else if (online_count > 1
            && (opts->record_filename || opts->kill_adb_on_close
#ifdef HAVE_V4L2
                || opts->v4l2_device
#endif
               )) {
        // Shared output files/sinks would conflict, and killing the shared ADB
        // server when one window closes would disconnect all the others.
        LOGE("Recording, V4L2 and --kill-adb-on-close require an explicit "
             "device (-s) when multiple devices are online");
    } else {
        LOGI("Opening %" SC_PRIsizet " online device(s)", online_count);
        *exit_code = launch_devices(argc, argv, opts, &devices);
    }
    sc_adb_devices_destroy(&devices);
    return true;
}
