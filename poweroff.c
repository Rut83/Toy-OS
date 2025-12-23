#include <signal.h>
#include <unistd.h>

int main(void)
{
    kill(1, SIGTERM);
    return 0;
}
