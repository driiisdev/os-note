// msh - a mini shell. everything a real shell does to run a command is here:
// fork() a child, wire up pipes with dup2(), execvp() the program, waitpid() for it,
// and keep Ctrl+C away from the shell itself.
//
//   supports:  cmd args...   cmd1 | cmd2 | cmd3   cmd &   cd dir   exit [code]
//   MSH_TRACE=1 ./bin/msh    prints every fork/exec/wait the shell performs
#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_ARGS 64
#define MAX_CMDS 8

static int trace;

static void tracef(const char *fmt, ...) {
    if (!trace) return;
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "\033[2m[msh] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\033[0m\n");
    va_end(ap);
}

// splits "ls -l | grep x &" into commands at each "|" token. returns the command count.
static int parse(char *line, char *cmds[MAX_CMDS][MAX_ARGS], int *background) {
    int ncmd = 0, narg = 0;
    *background = 0;
    for (char *tok = strtok(line, " \t\n"); tok; tok = strtok(NULL, " \t\n")) {
        if (strcmp(tok, "|") == 0) {
            if (narg == 0) return -1;            // "| foo" or "foo | | bar"
            cmds[ncmd++][narg] = NULL;
            narg = 0;
            if (ncmd == MAX_CMDS) return -1;
        } else if (strcmp(tok, "&") == 0) {
            *background = 1;
        } else if (narg < MAX_ARGS - 1) {
            cmds[ncmd][narg++] = tok;
        }
    }
    if (narg == 0) return ncmd == 0 ? 0 : -1;    // empty line, or a trailing "|"
    cmds[ncmd++][narg] = NULL;
    return ncmd;
}

// builtins have to run inside the shell process itself - `cd` in a child would only
// change the child's working directory, which disappears when the child exits
static int builtin(char **argv, int *status) {
    if (strcmp(argv[0], "exit") == 0) exit(argv[1] ? atoi(argv[1]) : *status);
    if (strcmp(argv[0], "cd") == 0) {
        const char *dir = argv[1] ? argv[1] : getenv("HOME");
        *status = chdir(dir) == 0 ? 0 : (perror("cd"), 1);
        return 1;
    }
    return 0;
}

static int run_pipeline(char *cmds[MAX_CMDS][MAX_ARGS], int n, int background) {
    pid_t pids[MAX_CMDS];
    int prev_read = -1;  // read end of the previous command's pipe

    for (int i = 0; i < n; i++) {
        int fd[2] = { -1, -1 };
        if (i < n - 1 && pipe(fd) < 0) { perror("pipe"); return 1; }

        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return 1; }
        if (pid == 0) {
            // ---- child: becomes command i -------------------------------------------
            signal(SIGINT, SIG_DFL);                 // Ctrl+C should kill the command
            if (prev_read != -1) { dup2(prev_read, STDIN_FILENO); close(prev_read); }
            if (fd[1] != -1) { dup2(fd[1], STDOUT_FILENO); close(fd[1]); close(fd[0]); }
            execvp(cmds[i][0], cmds[i]);             // only returns if it failed
            fprintf(stderr, "msh: %s: %s\n", cmds[i][0], strerror(errno));
            _exit(127);
        }
        // ---- parent: keep only the read end the NEXT command needs --------------------
        tracef("fork() -> child pid %d will exec \"%s\"", pid, cmds[i][0]);
        pids[i] = pid;
        if (prev_read != -1) close(prev_read);
        if (fd[1] != -1) close(fd[1]);   // if the shell kept this open, the reader never sees EOF
        prev_read = fd[0];
    }

    if (background) {
        printf("[bg] pid %d\n", pids[n - 1]);
        return 0;
    }
    int status = 0;
    for (int i = 0; i < n; i++) {
        int st;
        waitpid(pids[i], &st, 0);
        if (WIFEXITED(st)) {
            status = WEXITSTATUS(st);
            tracef("waitpid(%d) -> exited with %d", pids[i], status);
        } else if (WIFSIGNALED(st)) {
            status = 128 + WTERMSIG(st);
            tracef("waitpid(%d) -> killed by signal %d", pids[i], WTERMSIG(st));
        }
    }
    return status;  // like bash: the exit status of the LAST command in the pipeline
}

int main(void) {
    trace = getenv("MSH_TRACE") != NULL;
    signal(SIGINT, SIG_IGN);  // Ctrl+C kills the foreground command, not the shell
    int interactive = isatty(STDIN_FILENO);
    char *line = NULL;
    size_t cap = 0;
    int status = 0;

    for (;;) {
        // reap finished background jobs so they don't linger as zombies
        int st;
        pid_t done;
        while ((done = waitpid(-1, &st, WNOHANG)) > 0) printf("[done] pid %d\n", done);

        if (interactive) { printf("msh [%d]> ", status); fflush(stdout); }
        if (getline(&line, &cap, stdin) < 0) break;   // Ctrl+D / end of script

        char *cmds[MAX_CMDS][MAX_ARGS];
        int background;
        int n = parse(line, cmds, &background);
        if (n < 0) { fprintf(stderr, "msh: syntax error\n"); status = 2; continue; }
        if (n == 0) continue;
        if (n == 1 && builtin(cmds[0], &status)) continue;
        status = run_pipeline(cmds, n, background);
    }
    free(line);
    return status;
}
