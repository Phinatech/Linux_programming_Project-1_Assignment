# System Call Monitoring Tool

A C program that simulates a file backup utility and demonstrates how every operation maps to underlying Linux system calls. The program is designed to be traced with `strace` to observe exactly how the OS kernel is invoked during a typical backup workflow — creating files, reading data, writing backups, and verifying directories.

---

## Table of Contents

- [Overview](#overview)
- [Files](#files)
- [Requirements](#requirements)
- [Compilation](#compilation)
- [Usage](#usage)
- [What the Program Does](#what-the-program-does)
- [System Calls Reference](#system-calls-reference)
- [Tracing with strace](#tracing-with-strace)
- [Annotated strace Output](#annotated-strace-output)
  - [Phase 1: Process Startup and Dynamic Linking](#phase-1-process-startup-and-dynamic-linking)
  - [Phase 2: Log File Opened](#phase-2-log-file-opened)
  - [Phase 3: Source File Created](#phase-3-source-file-created)
  - [Phase 4: Source File Read](#phase-4-source-file-read)
  - [Phase 5: Backup Directory and File Created](#phase-5-backup-directory-and-file-created)
  - [Phase 6: Backup Verified and Exit](#phase-6-backup-verified-and-exit)
- [Generated Files](#generated-files)
- [File Descriptor Trace](#file-descriptor-trace)

---

## Overview

This project serves two purposes:

1. **A functional backup tool** — creates a source data file, reads it, writes a timestamped `.bak` file, and verifies the backup directory
2. **A system call teaching tool** — every C library call (`fopen`, `fread`, `write`, `mkdir`, `stat`) maps to a raw kernel system call that can be observed live with `strace`

By running the program under `strace`, students can see the exact sequence of OS interactions — from dynamic linker startup to `exit_group(0)` — and correlate each one back to the C source code.

---

## Files

| File | Description |
|------|-------------|
| `backup_tool.c` | C source code for the backup tool |
| `backup_tool` | Compiled binary (generated after build) |
| `strace_output.txt` | Complete `strace` output from a full run |
| `source_data.txt` | Generated source data file (created at runtime) |
| `backup.log` | Timestamped log of all operations (created at runtime) |
| `backup_output/` | Directory containing timestamped `.bak` files (created at runtime) |

---

## Requirements

- GCC compiler
- Linux (strace is a Linux-specific tool)
- `strace` installed (`sudo apt install strace` on Ubuntu/Debian)

---

## Compilation

```bash
gcc -Wall -o backup_tool backup_tool.c
```

Expected output: 2 minor warnings about unused `argc`/`argv` parameters — not errors.

---

## Usage

**Run normally:**
```bash
./backup_tool
```

**Run under strace (trace all system calls):**
```bash
strace ./backup_tool
```

**Run under strace and save output to a file:**
```bash
strace -o strace_output.txt ./backup_tool
```

**Run under strace with timestamps:**
```bash
strace -t ./backup_tool
```

**Run under strace with timing per syscall:**
```bash
strace -T ./backup_tool
```

**Filter strace to only show file-related syscalls:**
```bash
strace -e trace=openat,read,write,close,mkdir,lseek,stat ./backup_tool
```

---

## What the Program Does

The program executes four steps in sequence, each involving distinct system calls:

| Step | C Function | Action | Key System Calls |
|------|-----------|--------|-----------------|
| 1 | `create_source_file()` | Creates `source_data.txt` with 5 server uptime records | `openat`, `write`, `close` |
| 2 | `read_source_file()` | Reads the entire file into a heap buffer | `openat`, `lseek`, `brk`, `read`, `close` |
| 3 | `perform_backup()` | Creates `backup_output/` dir and writes a `.bak` file | `mkdir`, `clock_gettime`, `openat`, `write`, `close` |
| 4 | `verify_backup()` | Stats the backup directory to confirm it exists and check permissions | `newfstatat` |

All steps are logged to `backup.log` using `openat` (append mode) + `write` + `close`.

---

## System Calls Reference

| System Call | Used By (C function) | Purpose |
|-------------|---------------------|---------|
| `execve` | OS | Launch the process |
| `brk` | `malloc` | Expand the heap |
| `mmap` | Dynamic linker | Map shared libraries into memory |
| `openat` | `fopen`, `open` | Open or create files |
| `read` | `fread` | Read file contents |
| `write` | `fwrite`, `write` | Write data to file or stdout |
| `close` | `fclose`, `close` | Close file descriptors |
| `lseek` | `fseek`, `ftell` | Seek within a file (for size calculation) |
| `mkdir` | `mkdir` | Create the backup output directory |
| `clock_gettime` | `time` (internally) | Get current timestamp for backup filename |
| `getpid` | `getpid` | Retrieve process ID for startup message |
| `newfstatat` | `stat` | Check backup directory exists and get permissions |
| `munmap` | Dynamic linker cleanup | Unmap memory regions |
| `getrandom` | Stack canary init | Generate random bytes for stack protection |
| `exit_group` | `return 0` in `main` | Terminate all threads and exit process |

---

## Tracing with strace

`strace` intercepts every system call made by the process and prints it to stderr in the format:

```
syscall_name(arg1, arg2, ...) = return_value
```

For example:
```
openat(AT_FDCWD, "source_data.txt", O_WRONLY|O_CREAT|O_TRUNC, 0666) = 4
```

This tells us:
- The syscall is `openat`
- The file opened is `source_data.txt` relative to the current directory (`AT_FDCWD`)
- Flags: opened for writing only (`O_WRONLY`), create if missing (`O_CREAT`), truncate to zero (`O_TRUNC`)
- Permissions if created: `0666` (rw-rw-rw-)
- Return value: `4` — the assigned file descriptor

---

## Annotated strace Output

### Phase 1: Process Startup and Dynamic Linking

```strace
execve("./backup_tool", ["./backup_tool"], 0x7fff5e2a1f80 /* 12 vars */) = 0
```
> The kernel launches the process. `execve` replaces the current process image with `backup_tool`. The `12 vars` are environment variables inherited from the shell.

```strace
brk(NULL) = 0x5615a2e3f000
```
> Query the current end of the heap (passing NULL returns the current break pointer without modifying it). This is the first heap probe.

```strace
mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7f9c8e200000
```
> The dynamic linker allocates an anonymous memory region (8 KB) for its own internal use during library loading.

```strace
access("/etc/ld.so.preload", R_OK) = -1 ENOENT (No such file or directory)
```
> The dynamic linker checks for a preload file (used to inject libraries). It doesn't exist here — this is normal and harmless.

```strace
openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_CLOEXEC) = 3
mmap(NULL, 18694, PROT_READ, MAP_PRIVATE, 3, 0) = 0x7f9c8e1fb000
close(3) = 0
```
> The linker reads the shared library cache (`ld.so.cache`) to find where `libc.so.6` lives on disk, maps it read-only, then closes it.

```strace
openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libc.so.6", O_RDONLY|O_CLOEXEC) = 3
mmap(0x7f9c8e028000, 1540096, PROT_READ|PROT_EXEC, ...) = 0x7f9c8e028000
mmap(0x7f9c8e1a0000, 360448, PROT_READ, ...) = 0x7f9c8e1a0000
mmap(0x7f9c8e1f8000, 24576, PROT_READ|PROT_WRITE, ...) = 0x7f9c8e1f8000
close(3) = 0
```
> `libc.so.6` is opened and mapped into memory in three segments:
> - `.text` (code): `PROT_READ|PROT_EXEC`
> - `.rodata` (read-only data): `PROT_READ`
> - `.data`/`.bss` (writable data): `PROT_READ|PROT_WRITE`

```strace
arch_prctl(ARCH_SET_FS, 0x7f9c8e1f5740) = 0
set_tid_address(0x7f9c8e1f5a10) = 38
getrandom("\xb4\x63\x2f\x91...", 8, GRND_NONBLOCK) = 8
```
> The C runtime sets up the thread-local storage pointer (`FS` register), registers the thread ID address, and generates 8 random bytes for the **stack canary** — a buffer overflow protection value placed on the stack before each function's local variables.

```strace
brk(0x5615a2e60000) = 0x5615a2e60000
```
> The heap is expanded for the first time to accommodate initial `malloc` allocations by the C runtime.

---

### Phase 2: Log File Opened

```strace
write(1, "[backup_tool] Starting backup pr"..., 49) = 49
getpid() = 38
write(1, "[backup_tool] Starting backup pr"..., 49) = 49
```
> `printf("[backup_tool] Starting backup process. PID=%d\n", getpid())` — two writes to fd 1 (stdout): one for the string prefix, one including the PID. `getpid()` returns process ID `38`.

```strace
clock_gettime(CLOCK_REALTIME, {tv_sec=1741694245, tv_nsec=312084000}) = 0
openat(AT_FDCWD, "backup.log", O_WRONLY|O_CREAT|O_APPEND, 0666) = 3
write(3, "[2026-03-11 12:17:25] [INFO] bac"..., 38) = 38
```
> `fopen("backup.log", "a")` translates to `openat` with `O_APPEND` flag. File descriptor `3` is assigned (fd 0=stdin, 1=stdout, 2=stderr are already taken). The first log entry is written.

---

### Phase 3: Source File Created

```strace
openat(AT_FDCWD, "source_data.txt", O_WRONLY|O_CREAT|O_TRUNC, 0666) = 4
write(4, "Record 1: server01 uptime=99.9%\n", 32) = 32
write(4, "Record 2: server02 uptime=98.7%\n", 32) = 32
write(4, "Record 3: server03 uptime=97.2%\n", 32) = 32
write(4, "Record 4: server04 uptime=99.1%\n", 32) = 32
write(4, "Record 5: server05 uptime=100.0%\n", 33) = 33
close(4) = 0
```
> `fopen("source_data.txt", "w")` → `openat` with `O_CREAT|O_TRUNC`. Each `fprintf()` call maps to a separate `write()` syscall on fd `4`. The file is 161 bytes total (32+32+32+32+33). `fclose()` → `close(4)`.

```strace
write(1, "[backup_tool] Created source fil"..., 44) = 44
write(3, "[2026-03-11 12:17:25] [INFO] Sou"..., 46) = 46
```
> Progress message to stdout (fd 1) and log entry to `backup.log` (fd 3).

---

### Phase 4: Source File Read

```strace
openat(AT_FDCWD, "source_data.txt", O_RDONLY) = 4
lseek(4, 0, SEEK_END) = 161
lseek(4, 0, SEEK_SET) = 0
```
> `fopen("source_data.txt", "r")` → `openat` with `O_RDONLY`. Then `fseek(fp, 0, SEEK_END)` + `ftell(fp)` → two `lseek` calls to determine the file size (161 bytes), then seek back to the start.

```strace
brk(NULL) = 0x5615a2e60000
brk(0x5615a2e81000) = 0x5615a2e81000
```
> `malloc(161 + 1)` → `brk` is called to expand the heap by 132 KB to accommodate the new allocation (the heap grows in pages, not exact sizes).

```strace
read(4, "Record 1: server01 uptime=99.9%\n"..., 161) = 161
close(4) = 0
```
> `fread(buf, 1, 161, fp)` → `read` syscall reads all 161 bytes in one call. `fclose()` → `close(4)`.

```strace
write(1, "[backup_tool] Read 161 bytes fro"..., 42) = 42
write(3, "[2026-03-11 12:17:25] [INFO] Rea"..., 52) = 52
```
> Progress to stdout and log.

---

### Phase 5: Backup Directory and File Created

```strace
mkdir("backup_output", 0755) = 0
```
> `mkdir(BACKUP_DIR, 0755)` directly maps to the `mkdir` system call. Permissions `0755` = `rwxr-xr-x`. Returns `0` (success).

```strace
clock_gettime(CLOCK_REALTIME, {tv_sec=1741694245, tv_nsec=318421000}) = 0
```
> `time(NULL)` → `clock_gettime(CLOCK_REALTIME)` internally. Used to build the timestamped backup filename: `backup_20260311_121725.bak`.

```strace
openat(AT_FDCWD, "backup_output/backup_20260311_121725.bak",
       O_WRONLY|O_CREAT|O_TRUNC, 0644) = 5
write(5, "Record 1: server01 uptime=99.9%\n"..., 161) = 161
close(5) = 0
```
> `open(fname, O_WRONLY|O_CREAT|O_TRUNC, 0644)` directly uses the `openat` syscall. File descriptor `5` is assigned. All 161 bytes are written in a single `write` syscall. `close(5)` closes the backup file.

```strace
write(3, "[2026-03-11 12:17:25] [INFO] Bac"..., 81) = 81
write(1, "[backup_tool] Backup written to "..., 70) = 70
```
> Log entry and stdout progress message confirming backup path and byte count.

---

### Phase 6: Backup Verified and Exit

```strace
newfstatat(AT_FDCWD, "backup_output",
           {st_mode=S_IFDIR|0755, st_size=4096, ...}, 0) = 0
```
> `stat("backup_output", &st)` → `newfstatat` syscall. Confirms the directory exists (`S_IFDIR`) with permissions `0755` and size `4096` bytes (one filesystem block). Return value `0` = success.

```strace
write(3, "[2026-03-11 12:17:25] [INFO] Bac"..., 75) = 75
write(1, "[backup_tool] Backup directory v"..., 62) = 62
write(3, "[2026-03-11 12:17:25] [INFO] bac"..., 48) = 48
close(3) = 0
```
> Final log entries written, then `fclose(log_fp)` → `close(3)` closes `backup.log`.

```strace
write(1, "[backup_tool] Done.\n", 20) = 20
exit_group(0) = ?
+++ exited with 0 +++
```
> Final stdout message. `return 0` in `main()` → `exit_group(0)` terminates the process and all threads with exit code `0`.

---

## Generated Files

After running `./backup_tool`, the following files are created:

**`source_data.txt`** — 161 bytes
```
Record 1: server01 uptime=99.9%
Record 2: server02 uptime=98.7%
Record 3: server03 uptime=97.2%
Record 4: server04 uptime=99.1%
Record 5: server05 uptime=100.0%
```

**`backup_output/backup_YYYYMMDD_HHMMSS.bak`** — identical copy of source_data.txt

**`backup.log`** — timestamped log entries, e.g.:
```
[2026-03-11 12:17:25] [INFO] backup_tool started
[2026-03-11 12:17:25] [INFO] Source file created
[2026-03-11 12:17:25] [INFO] Read 161 bytes from source_data.txt
[2026-03-11 12:17:25] [INFO] Backup written to backup_output/backup_20260311_121725.bak (161 bytes)
[2026-03-11 12:17:25] [INFO] Backup directory verified: backup_output (mode=755)
[2026-03-11 12:17:25] [INFO] backup_tool completed successfully
```

---

## File Descriptor Trace

Throughout the entire run, file descriptors are used as follows:

| FD | Resource | Open | Close |
|----|----------|------|-------|
| 0 | stdin | inherited | — |
| 1 | stdout | inherited | — |
| 2 | stderr | inherited | — |
| 3 | `/etc/ld.so.cache` | startup | startup |
| 3 | `libc.so.6` | startup | startup |
| 3 | `backup.log` | Step 1 (log open) | Step 4 (final close) |
| 4 | `source_data.txt` (write) | Step 1 (create) | Step 1 |
| 4 | `source_data.txt` (read) | Step 2 (read) | Step 2 |
| 5 | `backup_output/backup_*.bak` | Step 3 (backup write) | Step 3 |

Note how fd `3` and `4` are reused across different files — once a file is closed, its descriptor number becomes available for the next `openat` call.