#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <linux/reboot.h>

/* ---------- Mount core filesystems ---------- */
static void mount_fs(void)
{
    mount("proc",     "/proc", "proc",     0, NULL);
    mount("sysfs",    "/sys",  "sysfs",    0, NULL);
    mount("devtmpfs", "/dev",  "devtmpfs", 0, NULL);
}

/* ---------- Reap non-shell zombies ---------- */
static void reap_background(void)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

/* ---------- Main ---------- */
int main(void)
{
    pid_t shell_pid;
    int status;

    /* PID 1 should ignore terminal signals */
    signal(SIGINT,  SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);

    mount_fs();

    /* Start the main system shell */
    shell_pid = fork();
    if (shell_pid == 0) {
        execl("/bin/sh", "sh", NULL);
        _exit(127);
    }

    /* ---------- PID 1 event loop ---------- */
    for (;;) {
        pid_t pid = wait(&status);

        if (pid < 0) {
            if (errno == EINTR)
                continue;
            pause();
            continue;
        }

        /* Shell exited → shutdown system */
        if (pid == shell_pid) {
            sync();
            reboot(LINUX_REBOOT_CMD_POWER_OFF);

            /* Never exit PID 1 */
            for (;;)
                pause();
        }

        /* Any other child → just reap */
        reap_background();
    }
}
