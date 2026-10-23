/* POSIX program launcher for the command line's `!`, plan M1.2 phase 5 (row I77). */
#include "../spawn.h"

#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define SPAWN_ARGS 64u
#define SPAWN_LINE 256u

int txt_spawn(const char* line, const char* const* paths, uint32_t n) {
    char buf[SPAWN_LINE];
    char* argv[SPAWN_ARGS + 1u];
    uint32_t argc = 0, i;
    char* p;
    pid_t pid;
    int status = 0;
    if (strlen(line) >= sizeof buf) {
        return -1;
    }
    strcpy(buf, line);
    for (p = strtok(buf, " \t"); p != NULL && argc < SPAWN_ARGS; p = strtok(NULL, " \t")) {
        argv[argc++] = p;
    }
    if (argc == 0 || argc + n > SPAWN_ARGS) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        argv[argc++] = (char*)(void*)paths[i];
    }
    argv[argc] = NULL;
    pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        execvp(argv[0], argv);
        _exit(127);
    }
    if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status)) {
        return -1;
    }
    return WEXITSTATUS(status) == 127 ? -1 : WEXITSTATUS(status);
}
