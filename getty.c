#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <signal.h>

int main(int argc, char *argv[])
{
    const char *tty = "/dev/tty1";

    if (argc > 1)
        tty = argv[1];

    /* Become session leader (required for job control) */
    setsid();

    /* Open terminal */
    int fd = open(tty, O_RDWR);
    if (fd < 0)
        _exit(1);

    /* Make this terminal the controlling TTY */
    ioctl(fd, TIOCSCTTY, 0);

    /* Attach stdin, stdout, stderr */
    dup2(fd, 0);
    dup2(fd, 1);
    dup2(fd, 2);
    if (fd > 2)
        close(fd);

    /* Reset signal handlers for interactive shell */
    signal(SIGINT,  SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);

    /* Start shell */
    setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin", 1);

    execv("/bin/bash", (char *[]){"bash", "--login", NULL});

    _exit(127);
}
    