#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void handler(int sig, siginfo_t *siginfo, void *ucontext) {
    // sig and si_signo should be the same
    printf("\rGot signal: sig=%d, siginfo->si_signo=%d, ucontext=%p!\n", sig, siginfo->si_signo, ucontext);
}

int main() {
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_SIGINFO;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction 1");
        return EXIT_FAILURE;
    }
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("sigaction 2");
        return EXIT_FAILURE;
    }

    struct sigaction sa_read;
    if (sigaction(SIGINT, NULL, &sa_read) == -1) {
        perror("sigaction 3");
        return EXIT_FAILURE;
    }

    printf("Installed signal handler: %p (flags: %d)\n", (void *)sa_read.sa_sigaction, sa_read.sa_flags);

    pause();

    return EXIT_SUCCESS;
}
