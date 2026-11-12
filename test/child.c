/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* The program the process suite starts. The first argument picks what it
   does:

       echo            copies stdin to stdout
       out TEXT        writes TEXT to stdout
       err TEXT        writes TEXT to stderr
       both TEXT       TEXT to stdout, then to stderr
       exit N          exits with N
       env NAME        writes the value of NAME, or "<unset>"
       cwd             writes the current directory
       args ...        writes each following argument and a NUL
       big N           writes N bytes to stdout and N to stderr, in turns
       sleep MS        sleeps MS milliseconds
       close_stdin     closes stdin, sleeps a little, exits 0
       ids             writes "pid ppid"
       pgid            writes "1" if it leads its process group, else "0"
*/

#include "../src/internal.h"

#include <cpolyfill/fs.h>
#include <cpolyfill/process.h>
#include <cpolyfill/stdlib.h>
#include <cpolyfill/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if PF_OS_WINDOWS
    #include <fcntl.h>
    #include <io.h>
#else
    #include <unistd.h>
#endif

int main(int argc, char *argv[]) {
    const char *mode = argc > 1 ? argv[1] : "";
    const char *arg = argc > 2 ? argv[2] : "";

#if PF_OS_WINDOWS
    _setmode(0, _O_BINARY);
    _setmode(1, _O_BINARY);
    _setmode(2, _O_BINARY);
#endif

    if (strcmp(mode, "echo") == 0) {
        char buf[8192];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), stdin)) > 0)
            if (fwrite(buf, 1, n, stdout) != n)
                return 1;
        return 0;
    }
    if (strcmp(mode, "out") == 0)
        return fputs(arg, stdout) < 0;
    if (strcmp(mode, "err") == 0)
        return fputs(arg, stderr) < 0;
    if (strcmp(mode, "both") == 0) {
        fputs(arg, stdout);
        fflush(stdout);
        fputs(arg, stderr);
        return 0;
    }
    if (strcmp(mode, "exit") == 0)
        return atoi(arg);
    if (strcmp(mode, "env") == 0) {
        char value[1024];
        if (pf_getenv(arg, value, sizeof(value)) < 0)
            fputs("<unset>", stdout);
        else
            fputs(value, stdout);
        return 0;
    }
    if (strcmp(mode, "cwd") == 0) {
        char cwd[PF_PATH_MAX];
        if (!pf_getcwd(cwd, sizeof(cwd)))
            return 1;
        fputs(cwd, stdout);
        return 0;
    }
    if (strcmp(mode, "args") == 0) {
        int k;
        for (k = 2; k < argc; k++)
            fwrite(argv[k], 1, strlen(argv[k]) + 1, stdout);
        return 0;
    }
    if (strcmp(mode, "big") == 0) {
        static char block[4096];
        long left = atol(arg);
        memset(block, 'x', sizeof(block));
        while (left > 0) {
            size_t n = left < (long)sizeof(block) ? (size_t)left : sizeof(block);
            if (fwrite(block, 1, n, stdout) != n || fwrite(block, 1, n, stderr) != n)
                return 1;
            left -= (long)n;
        }
        return 0;
    }
    if (strcmp(mode, "sleep") == 0) {
        pf_sleep_ns((uint64_t)atol(arg) * 1000000u);
        return 0;
    }
    if (strcmp(mode, "close_stdin") == 0) {
        fclose(stdin);
#if !PF_OS_WINDOWS
        close(0);
#endif
        pf_sleep_ns(100000000);
        return 0;
    }
    if (strcmp(mode, "ids") == 0) {
        printf("%lu %lu", (unsigned long)pf_getpid(), (unsigned long)pf_getppid());
        return 0;
    }
    if (strcmp(mode, "pgid") == 0) {
#if PF_OS_WINDOWS
        fputs("0", stdout);
#else
        fputs(getpgrp() == getpid() ? "1" : "0", stdout);
#endif
        return 0;
    }
    fprintf(stderr, "child: unknown mode '%s'\n", mode);
    return 2;
}
