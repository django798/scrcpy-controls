/* Copyright (C) 2026 moasm */
/* SPDX-License-Identifier: Apache-2.0 */
#include "common.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "adb/adb.h"
#include "adb/adb_parser.h"
#include "multi_device.h"
#include "util/file.h"

#define MAX_CHILDREN 8
#define MAX_ARGS 32

static const char *adb_output;
static bool list_ok;
static unsigned list_calls;
static unsigned adb_inits;
static unsigned adb_destroys;
static unsigned intr_inits;
static unsigned intr_destroys;
static unsigned launched;
static unsigned waited;
static int fail_spawn;
static struct {
    char *argv[MAX_ARGS];
    unsigned argc;
    sc_exit_code exit_code;
} children[MAX_CHILDREN];

bool
sc_adb_init(void) {
    ++adb_inits;
    return true;
}

void
sc_adb_destroy(void) {
    ++adb_destroys;
}

bool
sc_intr_init(struct sc_intr *intr) {
    (void) intr;
    ++intr_inits;
    return true;
}

void
sc_intr_destroy(struct sc_intr *intr) {
    (void) intr;
    ++intr_destroys;
}

bool
sc_adb_list_devices(struct sc_intr *intr, unsigned flags,
                     struct sc_vec_adb_devices *devices) {
    (void) intr;
    (void) flags;
    ++list_calls;
    if (!list_ok) {
        return false;
    }
    // Exercise the real ADB parser: do not fabricate parsed device objects.
    char *output = strdup(adb_output);
    assert(output);
    bool ok = sc_adb_parse_devices(output, devices);
    free(output);
    return ok;
}

char *
sc_file_get_executable_path(void) {
    return strdup("/path with spaces/scrcpy");
}

enum sc_process_result
sc_process_execute(const char *const argv[], sc_pid *pid, unsigned flags) {
    assert(flags == 0); // keep diagnostics visible in the launcher console
    assert(waited == 0); // every window must start before waiting for any exit
    unsigned index = launched++;
    assert(index < MAX_CHILDREN);
    for (unsigned i = 0; argv[i]; ++i) {
        assert(i < MAX_ARGS);
        children[index].argv[i] = strdup(argv[i]);
        assert(children[index].argv[i]);
        ++children[index].argc;
    }
    if ((int) index == fail_spawn) {
        return SC_PROCESS_ERROR_GENERIC;
    }
    *pid = (sc_pid) (intptr_t) (index + 1);
    return SC_PROCESS_SUCCESS;
}

sc_exit_code
sc_process_wait(sc_pid pid, bool close) {
    assert(close);
    unsigned index = (unsigned) (intptr_t) pid - 1;
    assert(index < launched);
    ++waited;
    return children[index].exit_code;
}

static void
reset(const char *output) {
    for (unsigned i = 0; i < launched; ++i) {
        for (unsigned j = 0; j < children[i].argc; ++j) {
            free(children[i].argv[j]);
        }
    }
    memset(children, 0, sizeof(children));
    adb_output = output;
    list_ok = true;
    list_calls = 0;
    adb_inits = adb_destroys = intr_inits = intr_destroys = 0;
    launched = waited = 0;
    fail_spawn = -1;
}

static void
assert_cleanup(void) {
    assert(adb_inits == adb_destroys);
    assert(intr_inits == intr_destroys);
}

static void
set_serial_env(const char *value) {
    // SDL's DLL may use another CRT on Windows. Change the same CRT
    // environment that production getenv() reads, not SDL's cached copy.
#ifdef _WIN32
    assert(!_putenv_s("ANDROID_SERIAL", value ? value : ""));
#else
    if (value) {
        assert(!setenv("ANDROID_SERIAL", value, 1));
    } else {
        assert(!unsetenv("ANDROID_SERIAL"));
    }
#endif
}

static void
test_concurrent_windows(void) {
    reset("List of devices attached\n"
          "board-A device product:board model:RayNeo device:mars\n"
          "not-ready unauthorized\n"
          "192.168.1.2:5555 device model:Pixel_7\n"
          "offline-board offline\n");
    struct scrcpy_options opts = scrcpy_options_default;
    char *argv[] = {"scrcpy", "--control-panel", "--no-audio", "--max-size=1280"};
    enum scrcpy_exit_code result;
    assert(sc_multi_device_run(ARRAY_LEN(argv), argv, &opts, &result));
    assert(result == SCRCPY_EXIT_SUCCESS);
    assert(launched == 2 && waited == 2);
    const char *serials[] = {"board-A", "192.168.1.2:5555"};
    const char *titles[] = {"RayNeo [board-A]", "Pixel_7 [192.168.1.2:5555]"};
    for (unsigned i = 0; i < 2; ++i) {
        assert(children[i].argc == 9);
        assert(!strcmp(children[i].argv[0], "/path with spaces/scrcpy"));
        assert(!strcmp(children[i].argv[1], "--serial"));
        assert(!strcmp(children[i].argv[2], serials[i]));
        assert(!strcmp(children[i].argv[3], "--no-terminal-title"));
        assert(!strcmp(children[i].argv[4], "--window-title"));
        assert(!strcmp(children[i].argv[5], titles[i]));
        for (unsigned j = 1; j < ARRAY_LEN(argv); ++j) {
            assert(!strcmp(children[i].argv[j + 5], argv[j]));
        }
        // The real child parses --serial, so it must not recursively spawn.
        struct scrcpy_options child_opts = opts;
        child_opts.serial = children[i].argv[2];
        assert(!sc_multi_device_run(children[i].argc, children[i].argv,
                                     &child_opts, &result));
    }
    assert(list_calls == 1);
    assert_cleanup();
}

static void
test_explicit_selection_and_non_mirroring(void) {
    for (unsigned i = 0; i < 9; ++i) {
        reset("List of devices attached\nA device\nB device\n");
        struct scrcpy_options opts = scrcpy_options_default;
        switch (i) {
            case 0: opts.serial = "A"; break;
            case 1: opts.select_usb = true; break;
            case 2: opts.select_tcpip = true; break;
            case 3: opts.tcpip_dst = "192.168.1.2:5555"; break;
            case 4: opts.list = SC_OPTION_LIST_APPS; break;
            case 5: opts.window = false; break;
            case 6: opts.video_playback = false; break;
            case 7:
#ifdef HAVE_USB
                opts.otg = true;
#else
                opts.list = SC_OPTION_LIST_DISPLAYS;
#endif
                break;
            case 8:
                set_serial_env("A");
                break;
        }
        char *argv[] = {"scrcpy"};
        enum scrcpy_exit_code result;
        assert(!sc_multi_device_run(ARRAY_LEN(argv), argv, &opts, &result));
        assert(!launched && !list_calls && !adb_inits);
        assert_cleanup();
        if (i == 8) {
            set_serial_env(NULL);
        }
    }
}

static void
test_single_online_and_unavailable_devices(void) {
    struct scrcpy_options opts = scrcpy_options_default;
    char *argv[] = {"scrcpy"};
    enum scrcpy_exit_code result;
    reset("List of devices attached\n");
    assert(!sc_multi_device_run(1, argv, &opts, &result));
    assert(!launched);
    assert_cleanup();
    reset("List of devices attached\nA device\n");
    assert(!sc_multi_device_run(1, argv, &opts, &result));
    assert(!launched);
    assert_cleanup();
    reset("List of devices attached\nA device\nB offline\n");
    assert(sc_multi_device_run(1, argv, &opts, &result));
    assert(result == SCRCPY_EXIT_SUCCESS && launched == 1 && waited == 1);
    assert(!strcmp(children[0].argv[2], "A"));
    assert(!strcmp(children[0].argv[5], "Android [A]"));
    assert_cleanup();
    reset("List of devices attached\nA unauthorized\nB offline\n");
    assert(sc_multi_device_run(1, argv, &opts, &result));
    assert(result == SCRCPY_EXIT_FAILURE && !launched);
    assert_cleanup();
    reset("");
    list_ok = false;
    assert(sc_multi_device_run(1, argv, &opts, &result));
    assert(result == SCRCPY_EXIT_FAILURE && !launched);
    assert_cleanup();
}

static void
test_failure_isolation(void) {
    struct scrcpy_options opts = scrcpy_options_default;
    char *argv[] = {"scrcpy"};
    for (unsigned i = 0; i < 3; ++i) {
        reset("List of devices attached\nA device\nB device\nC device\n");
        if (i == 0) {
            fail_spawn = 1;
        } else {
            children[1].exit_code = i == 1 ? SCRCPY_EXIT_FAILURE
                                            : SCRCPY_EXIT_DISCONNECTED;
        }
        enum scrcpy_exit_code result;
        assert(sc_multi_device_run(1, argv, &opts, &result));
        assert(launched == 3);
        assert(waited == (i == 0 ? 2 : 3));
        assert(result == (i == 2 ? SCRCPY_EXIT_DISCONNECTED : SCRCPY_EXIT_FAILURE));
        assert_cleanup();
    }
}

static void
test_custom_arguments_and_shared_outputs(void) {
    reset("List of devices attached\nA device\nB device\n");
    struct scrcpy_options opts = scrcpy_options_default;
    opts.window_title = "My \"test\" window";
    char *argv[] = {"scrcpy", "--window-title", "My \"test\" window", "--"};
    enum scrcpy_exit_code result;
    assert(sc_multi_device_run(ARRAY_LEN(argv), argv, &opts, &result));
    assert(launched == 2);
    for (unsigned i = 0; i < 2; ++i) {
        assert(children[i].argc == 7);
        for (unsigned j = 1; j < ARRAY_LEN(argv); ++j) {
            assert(!strcmp(children[i].argv[j + 3], argv[j]));
        }
    }
    assert_cleanup();

    for (unsigned i = 0; i < 2; ++i) {
        reset("List of devices attached\nA device\nB device\n");
        opts = scrcpy_options_default;
        if (i == 0) {
            opts.record_filename = "shared.mp4";
        } else {
            opts.kill_adb_on_close = true;
        }
        assert(sc_multi_device_run(1, argv, &opts, &result));
        assert(result == SCRCPY_EXIT_FAILURE && !launched);
        assert_cleanup();
    }
}

int
main(void) {
    const char *env_serial = getenv("ANDROID_SERIAL");
    char *saved_serial = env_serial ? strdup(env_serial) : NULL;
    set_serial_env(NULL);
    test_concurrent_windows();
    test_explicit_selection_and_non_mirroring();
    test_single_online_and_unavailable_devices();
    test_failure_isolation();
    test_custom_arguments_and_shared_outputs();
    reset("");
    if (saved_serial) {
        set_serial_env(saved_serial);
        free(saved_serial);
    }
    return 0;
}
