/*
 * monitor_service.c — Simple background service with Unix signal handling.
 *
 * Demonstrates:
 *   SIGINT  — graceful shutdown (Ctrl+C or kill -SIGINT <pid>)
 *   SIGUSR1 — administrator status request
 *   SIGTERM — emergency shutdown
 *
 * Compile:
 *   gcc -Wall -o monitor_service monitor_service.c
 *
 * Run in background:
 *   ./monitor_service &
 *   echo "PID: $!"
 *
 * Send signals:
 *   kill -SIGUSR1 <pid>
 *   kill -SIGINT  <pid>
 *   kill -SIGTERM <pid>
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

/* ── Global flag: set to 0 by signal handlers to break the main loop ── */
static volatile sig_atomic_t keep_running = 1;

/* ── Timestamp helper: writes "YYYY-MM-DD HH:MM:SS" into buf ── */
static void timestamp(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, len, "%Y-%m-%d %H:%M:%S", t);
}

/*
 * handle_sigint — SIGINT handler
 * Triggered by Ctrl+C or kill -SIGINT <pid>.
 * Sets keep_running = 0 so the main loop exits cleanly,
 * then prints a graceful shutdown message.
 *
 * Note: only async-signal-safe functions (write) are used here.
 * printf is not async-signal-safe; we use write() directly.
 */
static void handle_sigint(int sig) {
    (void)sig;   /* suppress unused-parameter warning */
    const char *msg =
        "\n[Monitor Service] SIGINT received — shutting down safely.\n"
        "[Monitor Service] Monitor Service shutting down safely.\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    keep_running = 0;
}

/*
 * handle_sigusr1 — SIGUSR1 handler
 * Triggered by kill -SIGUSR1 <pid>.
 * Prints a status message without terminating.
 */
static void handle_sigusr1(int sig) {
    (void)sig;
    const char *msg =
        "[Monitor Service] SIGUSR1 received — "
        "System status requested by administrator.\n";
    write(STDOUT_FILENO, msg, strlen(msg));
}

/*
 * handle_sigterm — SIGTERM handler
 * Triggered by kill -SIGTERM <pid> or kill <pid> (default signal).
 * Prints an emergency message and exits immediately.
 */
static void handle_sigterm(int sig) {
    (void)sig;
    const char *msg =
        "[Monitor Service] SIGTERM received — "
        "Emergency shutdown signal received.\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    /* SIGTERM semantics: immediate exit, no cleanup */
    _exit(EXIT_FAILURE);
}

/*
 * register_signals — Install all signal handlers using sigaction().
 *
 * sigaction() is preferred over signal() because:
 *   1. signal() behaviour is implementation-defined on some systems.
 *   2. sigaction() guarantees the handler stays installed after delivery
 *      (no need to re-register inside the handler).
 *   3. SA_RESTART causes interrupted system calls (like sleep/nanosleep)
 *      to resume automatically, preventing spurious EINTR errors.
 */
static void register_signals(void) {
    struct sigaction sa;

    /* SIGINT — graceful shutdown */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction(SIGINT)");
        exit(EXIT_FAILURE);
    }

    /* SIGUSR1 — status request */
    sa.sa_handler = handle_sigusr1;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("sigaction(SIGUSR1)");
        exit(EXIT_FAILURE);
    }

    /* SIGTERM — emergency shutdown */
    sa.sa_handler = handle_sigterm;
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction(SIGTERM)");
        exit(EXIT_FAILURE);
    }
}

int main(void) {
    char ts[32];

    /* Print PID so the user knows which process to signal */
    timestamp(ts, sizeof(ts));
    printf("[Monitor Service] Started at %s  PID=%d\n", ts, getpid());
    printf("[Monitor Service] Handlers registered: SIGINT | SIGUSR1 | SIGTERM\n");
    printf("[Monitor Service] Send signals with:\n");
    printf("  kill -SIGUSR1 %d\n", getpid());
    printf("  kill -SIGINT  %d\n", getpid());
    printf("  kill -SIGTERM %d\n\n", getpid());
    fflush(stdout);

    /* Install handlers BEFORE entering the loop */
    register_signals();

    /* ── Main service loop: print heartbeat every 5 seconds ── */
    while (keep_running) {
        timestamp(ts, sizeof(ts));
        printf("[Monitor Service] %s — System running normally...\n", ts);
        fflush(stdout);

        /*
         * sleep() is interrupted by signal delivery (EINTR).
         * SA_RESTART re-starts it automatically for SIGUSR1
         * (which does not set keep_running = 0).
         * For SIGINT the loop condition re-evaluates and exits.
         */
        sleep(5);
    }

    /* ── Clean exit path (reached only via SIGINT) ── */
    timestamp(ts, sizeof(ts));
    printf("[Monitor Service] %s — Cleanup complete. Goodbye.\n", ts);
    return EXIT_SUCCESS;
}
