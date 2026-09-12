# PS5 OpenGL - OpenGL implementation for PlayStation 5.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile the actual diagnostic sink and runtime against a fake kernel."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class NativeDiagnosticsTest(unittest.TestCase):
    def compile_run(self, source, runtime=False):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)
            (path / "test.c").write_text(source)
            command = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                       "-I" + str(ROOT / "native-app"), str(path / "test.c")]
            if runtime:
                command += ["-DPS5_NATIVE_BOUNDED_TRIANGLE=1",
                            str(ROOT / "native-app/runtime_shims.c")]
            subprocess.run(command + ["-o", str(path / "test")], check=True)
            subprocess.run([str(path / "test")], check=True, timeout=5)

    def test_format_bounds_and_sink_errors(self):
        self.compile_run(r"""
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
static int fail_format;
static int format_stub(char *s, size_t n, const char *f, va_list args) {
    return fail_format ? -1 : vsnprintf(s, n, f, args);
}
#define vsnprintf format_stub
#include "native_diagnostics.h"
#undef vsnprintf
static char captured[1024];
static int calls;
int sceKernelDebugOutText(int device, const char *text) {
    assert(device == 0);
    assert(memchr(text, 0, sizeof(captured)) != NULL);
    size_t n = strlen(text);
    assert(n > 0 && text[n-1] == '\n');
    memcpy(captured, text, n+1);
    ++calls;
    return -42;
}
int main(void) {
    assert(ps5_native_trace("OGL2_FRAME_COMPLETE frame=%u", 600u) == -42);
    assert(strcmp(captured, "OGL2_FRAME_COMPLETE frame=600\n") == 0);
    ps5_native_trace("%s", "already\n");
    assert(strcmp(captured, "already\n") == 0);
    ps5_native_trace("%s", "");
    assert(strcmp(captured, "\n") == 0);
    char long_text[2048];
    memset(long_text, 'x', sizeof(long_text)-1);
    long_text[sizeof(long_text)-1] = 0;
    ps5_native_trace("%s", long_text);
    assert(strlen(captured) == 1023);
    assert(strcmp(captured + 1023 - strlen(" [truncated]\n"), " [truncated]\n") == 0);
    long_text[1022] = 0;
    ps5_native_trace("%s", long_text);
    assert(strlen(captured) == 1023 && captured[1021] == 'x');
    fail_format = 1;
    ps5_native_trace("%s", "unused");
    assert(strcmp(captured, "[ps5-opengl-native] diagnostic-format-error\n") == 0);
    assert(calls == 6);
    return 0;
}
""")

    def test_runtime_exit_flow_ignores_sink_failure(self):
        self.compile_run(r"""
#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
extern void catchReturnFromMain(int status);
static jmp_buf stopped;
static int accepted, calls;
static char receipt[2048];
int sceKernelDebugOutText(int device, const char *text) {
    assert(device == 0);
    assert(strlen(receipt) + strlen(text) < sizeof(receipt));
    strcat(receipt, text);
    ++calls;
    return -42;
}
int sceSystemServiceLoadExec(const char *path, const char *const *argv) {
    assert(strcmp(path, "exit") == 0 && argv == NULL);
    assert(strstr(receipt, "OGL2_EXIT_REQUEST_BEGIN\n") != NULL);
    if (accepted) longjmp(stopped, 1);
    return -9;
}
int sceKernelUsleep(uint32_t usec) {
    assert(!accepted && usec == 100000);
    longjmp(stopped, 2);
}
int main(void) {
    for (accepted = 0; accepted < 2; ++accepted) {
        for (int status = 0; status <= 1; ++status) {
            calls = 0; receipt[0] = 0;
            int reason = setjmp(stopped);
            if (!reason) catchReturnFromMain(status);
            assert(reason == (accepted ? 1 : 2));
            assert(calls == (accepted ? 2 : 3));
            char expected[128];
            snprintf(expected, sizeof(expected), "gate completed status=%d\n", status);
            assert(strstr(receipt, expected) != NULL);
            assert((strstr(receipt, "OGL2_FAIL operation=exit-request result=0xfffffff7") != NULL) == !accepted);
        }
    }
    return 0;
}
""", runtime=True)

if __name__ == "__main__":
    unittest.main()
