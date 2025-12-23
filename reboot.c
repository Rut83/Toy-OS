#include <signal.h>
#include <unistd.h>

int main(void)
{
    kill(1, SIGUSR1);
    return 0;
}
