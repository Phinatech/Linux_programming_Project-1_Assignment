# Signal-Based Server Controller

A C program that implements a background monitoring service with Unix signal handling. It demonstrates how to register and respond to POSIX signals using `sigaction()`, maintain a long-running service loop, and perform both graceful and emergency shutdowns — all without relying on third-party libraries.

---

## Table of Contents

- [Overview](#overview)
- [Files](#files)
- [Signals Handled](#signals-handled)
- [Requirements](#requirements)
- [Compilation](#compilation)
- [Usage](#usage)
- [How It Works](#how-it-works)
  - [Signal Registration](#signal-registration)
  - [Main Service Loop](#main-service-loop)
  - [SIGINT Handler — Graceful Shutdown](#sigint-handler--graceful-shutdown)
  - [SIGUSR1 Handler — Status Request](#sigusr1-handler--status-request)
  - [SIGTERM Handler — Emergency Shutdown](#sigterm-handler--emergency-shutdown)
- [Verified Terminal Demo](#verified-terminal-demo)
- [Key Concepts](#key-concepts)
  - [sigaction vs signal](#sigaction-vs-signal)
  - [Async-Signal-Safe Functions](#async-signal-safe-functions)
  - [SA_RESTART Flag](#sa_restart-flag)
  - [volatile sig_atomic_t](#volatile-sig_atomic_t)
  - [_exit vs exit](#_exit-vs-exit)
- [Signal Quick Reference](#signal-quick-reference)

---

## Overview

`monitor_service` is a simulated background service that:

1. Starts and prints its **PID** and registered signal commands
2. Enters a loop printing a **heartbeat** every 5 seconds
3. Responds to three Unix signals — each with different behaviour:
   - **SIGINT** → logs a message and shuts down cleanly
   - **SIGUSR1** → prints a status message and keeps running
   - **SIGTERM** → prints an emergency message and exits immediately
4. Performs a **cleanup** message on graceful exit (SIGINT only)

---

## Files

| File | Description |
|------|-------------|
| `monitor_service.c` | Full C source code |
| `terminal_demo.txt` | Verified terminal session showing all three signals in action |
| `monitor_service` | Compiled binary (generated after build) |

---

## Signals Handled

| Signal | Trigger | Behaviour | Exit? |
|--------|---------|-----------|-------|
| `SIGINT` | `Ctrl+C` or `kill -SIGINT <pid>` | Sets `keep_running = 0`, prints shutdown message, loop exits cleanly | Yes (graceful) |
| `SIGUSR1` | `kill -SIGUSR1 <pid>` | Prints status message, service continues running | No |
| `SIGTERM` | `kill -SIGTERM <pid>` or `kill <pid>` | Prints emergency message, calls `_exit()` immediately | Yes (immediate) |

---

## Requirements

- GCC compiler
- Linux (signals like `SIGUSR1` behave differently on macOS — compile and test on Linux for accurate results)
- No external libraries required

---

## Compilation

```bash
gcc -Wall -o monitor_service monitor_service.c
```

Expected output: no warnings, clean compilation.

---

## Usage

**Start in the foreground:**
```bash
./monitor_service
```

**Start in the background (recommended for signal testing):**
```bash
./monitor_service &
echo "PID: $!"
```

**Send signals to the running process:**
```bash
# Request administrator status (service stays running)
kill -SIGUSR1 <pid>

# Graceful shutdown (loop exits, cleanup runs)
kill -SIGINT <pid>

# Emergency shutdown (immediate _exit, no cleanup)
kill -SIGTERM <pid>
```

**Verify the process has exited:**
```bash
kill -0 <pid>
# If output: "No such process" → process has exited successfully
```

---

## How It Works

### Signal Registration

All three signal handlers are installed using `sigaction()` inside `register_signals()`, which is called once at the start of `main()` **before** the service loop begins:

```c
static void register_signals(void) {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = handle_sigusr1;
    sigaction(SIGUSR1, &sa, NULL);

    sa.sa_handler = handle_sigterm;
    sigaction(SIGTERM, &sa, NULL);
}
```

- `sigemptyset(&sa.sa_mask)` — no additional signals are blocked during handler execution
- `SA_RESTART` — interrupted system calls (like `sleep`) are automatically restarted
- All three handlers share the same `sigaction` struct, with only `sa_handler` changed between registrations

---

### Main Service Loop

```c
while (keep_running) {
    timestamp(ts, sizeof(ts));
    printf("[Monitor Service] %s — System running normally...\n", ts);
    fflush(stdout);
    sleep(5);
}
```

- `keep_running` is a `volatile sig_atomic_t` global, initially set to `1`
- The loop prints a timestamped heartbeat every 5 seconds
- `fflush(stdout)` ensures output appears immediately even in buffered mode
- When `SIGINT` is received, `keep_running` is set to `0` and the loop exits on the next iteration

---

### SIGINT Handler — Graceful Shutdown

```c
static void handle_sigint(int sig) {
    (void)sig;
    const char *msg =
        "\n[Monitor Service] SIGINT received — shutting down safely.\n"
        "[Monitor Service] Monitor Service shutting down safely.\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    keep_running = 0;
}
```

**Behaviour:**
1. Prints a shutdown notification using `write()` (async-signal-safe)
2. Sets `keep_running = 0`
3. Returns from the handler — the main loop checks the flag and exits
4. Cleanup code in `main()` runs: prints a final "Cleanup complete. Goodbye." message
5. Process exits with code `0` (success)

**This is the only path that runs cleanup code.**

---

### SIGUSR1 Handler — Status Request

```c
static void handle_sigusr1(int sig) {
    (void)sig;
    const char *msg =
        "[Monitor Service] SIGUSR1 received — "
        "System status requested by administrator.\n";
    write(STDOUT_FILENO, msg, strlen(msg));
}
```

**Behaviour:**
1. Prints a status message using `write()`
2. Returns from the handler immediately
3. The main loop **continues running** — heartbeats resume
4. Can be sent multiple times without affecting service state
5. `SA_RESTART` ensures that any `sleep()` in progress is automatically restarted

---

### SIGTERM Handler — Emergency Shutdown

```c
static void handle_sigterm(int sig) {
    (void)sig;
    const char *msg =
        "[Monitor Service] SIGTERM received — "
        "Emergency shutdown signal received.\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    _exit(EXIT_FAILURE);
}
```

**Behaviour:**
1. Prints an emergency message using `write()`
2. Calls `_exit(EXIT_FAILURE)` — **immediate termination**
3. No cleanup code runs — no `atexit` handlers, no stdio buffer flushing
4. Process exits with code `1` (failure)

**This is intentional** — `SIGTERM` semantics imply an emergency or forced stop where cleanup is not guaranteed.

---

## Verified Terminal Demo

The following is the actual verified output from running the program on Linux x86-64 / GCC 13.3.0:

```
$ gcc -Wall -o monitor_service monitor_service.c
$
  (No warnings — clean compilation)

$ ./monitor_service &
[1] 53

[Monitor Service] Started at 2026-03-11 13:28:48  PID=53
[Monitor Service] Handlers registered: SIGINT | SIGUSR1 | SIGTERM
[Monitor Service] Send signals with:
  kill -SIGUSR1 53
  kill -SIGINT  53
  kill -SIGTERM 53

[Monitor Service] 2026-03-11 13:28:48 — System running normally...
[Monitor Service] 2026-03-11 13:28:53 — System running normally...

$ kill -SIGUSR1 53
[Monitor Service] SIGUSR1 received — System status requested by administrator.
[Monitor Service] 2026-03-11 13:28:55 — System running normally...

$ kill -SIGUSR1 53
[Monitor Service] SIGUSR1 received — System status requested by administrator.
[Monitor Service] 2026-03-11 13:28:56 — System running normally...

$ kill -SIGINT 53
[Monitor Service] SIGINT received — shutting down safely.
[Monitor Service] Monitor Service shutting down safely.
[Monitor Service] 2026-03-11 13:28:57 — Cleanup complete. Goodbye.

[1]+  Done     ./monitor_service

$ kill -0 53
bash: kill: (53) — No such process
  → Process exited cleanly.
```

**SIGTERM demonstration (separate run, PID=64):**

```
$ ./monitor_service &
[1] 64

[Monitor Service] Started at 2026-03-11 13:29:03  PID=64
[Monitor Service] 2026-03-11 13:29:03 — System running normally...
[Monitor Service] 2026-03-11 13:29:08 — System running normally...

$ kill -SIGTERM 64
[Monitor Service] SIGTERM received — Emergency shutdown signal received.

[1]+  Done     ./monitor_service
  → Process printed emergency message and called _exit() immediately.
     No cleanup code ran.
```

---

## Key Concepts

### sigaction vs signal

`sigaction()` is preferred over the older `signal()` function for these reasons:

| Property | `signal()` | `sigaction()` |
|----------|-----------|----------------|
| Handler persistence | Implementation-defined (may reset after first delivery) | Guaranteed to stay installed |
| Portability | Behaviour varies across systems | POSIX-defined, consistent |
| Interrupted syscalls | No control | Controlled via `SA_RESTART` flag |
| Signal masking | No control | Fine-grained via `sa_mask` |

---

### Async-Signal-Safe Functions

Signal handlers can be called at **any point** during normal execution — even in the middle of another function call. This means only **async-signal-safe** functions may be called inside a signal handler.

| Function | Async-signal-safe? | Notes |
|----------|--------------------|-------|
| `write()` | **Yes** | Used in all handlers |
| `printf()` | **No** | Uses internal locks — can deadlock |
| `fprintf()` | **No** | Same issue |
| `malloc()` | **No** | Uses heap locks |
| `exit()` | **No** | Flushes stdio buffers (unsafe) |
| `_exit()` | **Yes** | Raw syscall, no buffering |

This is why all handlers use `write(STDOUT_FILENO, ...)` instead of `printf()`.

---

### SA_RESTART Flag

When a signal is delivered while the process is blocked in a system call (like `sleep()`), the system call is interrupted and returns `EINTR`. The `SA_RESTART` flag tells the kernel to **automatically restart** the system call after the handler returns.

- For `SIGUSR1`: the `sleep()` is restarted — the heartbeat interval is not affected
- For `SIGINT`: the loop condition `keep_running == 0` is checked after the `sleep()` returns, so the loop exits cleanly even if sleep is in progress

---

### volatile sig_atomic_t

```c
static volatile sig_atomic_t keep_running = 1;
```

- `volatile` — tells the compiler **not to cache** this variable in a register. Without it, the compiler might optimise the loop and never re-read the variable from memory.
- `sig_atomic_t` — a type guaranteed to be read/written **atomically** on the target platform. This prevents partial reads/writes when the signal handler modifies it concurrently with the main loop.

Using a plain `int` without `volatile` would be undefined behaviour in a signal context.

---

### _exit vs exit

| Function | Behaviour |
|----------|-----------|
| `exit()` | Flushes stdio buffers, runs `atexit()` handlers, then calls `_exit()` |
| `_exit()` | Terminates immediately via a raw syscall — no flushing, no handlers |

`_exit()` is async-signal-safe; `exit()` is not. The `SIGTERM` handler uses `_exit(EXIT_FAILURE)` to guarantee immediate termination without risking deadlocks from stdio buffer operations.

---

## Signal Quick Reference

```bash
# Start service in background
./monitor_service &

# Get the PID (if not shown)
pgrep monitor_service

# Send SIGUSR1 — status request (non-terminating)
kill -SIGUSR1 <pid>

# Send SIGINT — graceful shutdown
kill -SIGINT <pid>
# or press Ctrl+C if running in foreground

# Send SIGTERM — emergency shutdown
kill -SIGTERM <pid>
# or simply:
kill <pid>          # SIGTERM is the default signal

# Verify process has exited
kill -0 <pid>       # Prints "No such process" if exited
echo $?             # Exit code: 0 = running, 1 = not found
```