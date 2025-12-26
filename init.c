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

static void mount_fs(void)
{
    mount("proc",     "/proc", "proc",     0, NULL);
    mount("sysfs",    "/sys",  "sysfs",    0, NULL);
    mount("devtmpfs", "/dev",  "devtmpfs", 0, NULL);
}

static pid_t spawn_getty(const char *tty)
{
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/getty", "getty", tty, NULL);

        _exit(127);
    }
    return pid;
}

int main(void)
{
    int status;
    write(1, ">>> CUSTOM INIT RUNNING <<<\n", 28);
    
    setup_signals();
    mount_fs();

    pid_t tty1 = spawn_getty("/dev/tty1");
    pid_t tty2 = spawn_getty("/dev/tty2");



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

    if (pid == tty1)
        tty1 = spawn_getty("/dev/tty1");

    else if (pid == tty2)
        tty2 = spawn_getty("/dev/tty2");


}

}
