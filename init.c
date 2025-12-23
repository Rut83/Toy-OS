#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <linux/reboot.h>

static volatile sig_atomic_t shutdown_requested = 0;

static volatile sig_atomic_t reboot_requested = 0;

static void handle_signal(int sig)
{
    if (sig == SIGTERM)
        shutdown_requested = 1;
    else if (sig == SIGUSR1)
        reboot_requested = 1;
}


static void setup_signals(void)
{
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
}

static void shutdown_system(int cmd)
{
    /* Ask all processes to exit */
    kill(-1, SIGTERM);

    sleep(1);   /* give them time */

    /* Force kill remaining processes */
    kill(-1, SIGKILL);

    /* Reap everything */
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;

    sync();
    reboot(cmd);

    for (;;)
        pause();
}

/* ---------- Mount core filesystems ---------- */
static void mount_fs(void)
{
    mount("proc",     "/proc", "proc",     0, NULL);
    mount("sysfs",    "/sys",  "sysfs",    0, NULL);
    mount("devtmpfs", "/dev",  "devtmpfs", 0, NULL);
}

/* ---------- Spawn shell ---------- */
static pid_t spawn_shell(void)
{
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", NULL);
        _exit(127);
    }
    return pid;
}

/* ---------- Main ---------- */
int main(void)
{
    pid_t shell_pid;
    int status;
    write(1, ">>> CUSTOM INIT RUNNING <<<\n", 28);


    setup_signals();


    mount_fs();
    shell_pid = spawn_shell();

    for (;;) {
    pid_t pid = wait(&status);

    if (pid < 0) {
        if (errno == EINTR) {
            if (shutdown_requested) {
                shutdown_system(LINUX_REBOOT_CMD_POWER_OFF);
            }
            if (reboot_requested) {
                shutdown_system(LINUX_REBOOT_CMD_RESTART);
            }
            continue;
        }
        continue;
    }

    if (pid == shell_pid) {
        shell_pid = spawn_shell();
    }
}

}
