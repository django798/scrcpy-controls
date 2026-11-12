/* Copyright (C) 2026 moasm */
/* SPDX-License-Identifier: Apache-2.0 */
#ifndef SC_MULTI_DEVICE_H
#define SC_MULTI_DEVICE_H

#include "common.h"

#include <stdbool.h>

#include "options.h"
#include "scrcpy.h"

// When no device was explicitly selected, start a separate scrcpy process for
// every online device. All processes are started before waiting for any of them.
// Returns false to continue the ordinary single-device startup. Otherwise,
// writes the final exit code after all launched windows have closed.
bool
sc_multi_device_run(int argc, char *const argv[],
                     const struct scrcpy_options *options,
                     enum scrcpy_exit_code *exit_code);

#endif
