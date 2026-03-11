# Investigating a Suspicious Binary

A binary analysis exercise that demonstrates how to investigate an unknown ELF executable using standard Linux forensic tools — without access to the original source code. The target binary (`data_sync`) is analysed from multiple angles: file type identification, symbol inspection, string extraction, disassembly, and dynamic library examination.

---

## Table of Contents

- [Overview](#overview)
- [Files](#files)
- [Tools Used](#tools-used)
- [Investigation Steps](#investigation-steps)
  - [1. File Type Identification](#1-file-type-identification)
  - [2. ELF Header Analysis](#2-elf-header-analysis)
  - [3. Section Headers](#3-section-headers)
  - [4. Dynamic Symbol Table](#4-dynamic-symbol-table)
  - [5. Full Symbol Table](#5-full-symbol-table)
  - [6. Shared Library Dependencies](#6-shared-library-dependencies)
  - [7. String Extraction](#7-string-extraction)
  - [8. Disassembly](#8-disassembly)
- [Findings Summary](#findings-summary)
- [Binary Behaviour Reconstruction](#binary-behaviour-reconstruction)
- [Running the Binary](#running-the-binary)

---

## Overview

This exercise simulates receiving an unknown compiled binary and performing static analysis to determine:

- What type of binary it is
- What functions it defines and imports
- What file paths, strings, and configuration values are embedded in it
- What the binary does, reconstructed purely from analysis — before running it

The binary investigated is `data_sync`, a 64-bit ELF Linux executable compiled from C.

---

## Files

| File | Description |
|------|-------------|
| `data_sync` | The compiled ELF binary under investigation |
| `data_sync.c` | The original C source code (for verification) |
| `command_outputs.txt` | Full output from all analysis commands |

---

## Tools Used

| Tool | Purpose |
|------|---------|
| `file` | Identify binary type and architecture |
| `readelf -h` | Parse the ELF header |
| `objdump -h` | List all ELF sections with sizes and offsets |
| `nm -D` | Show dynamic (imported/exported) symbols |
| `nm` | Show all symbols including internal functions |
| `ldd` | List shared library dependencies |
| `strings` | Extract printable strings from the binary |
| `objdump -d` | Disassemble the `.text` section |

---

## Investigation Steps

### 1. File Type Identification

```bash
file data_sync
```

**Output:**
```
data_sync: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV),
dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2,
BuildID[sha1]=743304f23bc629c8cbd6bb3c33e11b610f5390ec,
for GNU/Linux 3.2.0, not stripped
```

**Key findings:**
- `ELF 64-bit` — Linux executable, 64-bit
- `LSB` — Little-endian byte order (x86-64 standard)
- `PIE` — Position Independent Executable (ASLR-compatible)
- `dynamically linked` — depends on shared libraries at runtime
- `not stripped` — debug symbols are present (function names visible)
- Compiled for Linux kernel 3.2.0 or later
- Build ID: `743304f23bc629c8cbd6bb3c33e11b610f5390ec`

---

### 2. ELF Header Analysis

```bash
readelf -h data_sync
```

**Key fields from the ELF header:**

| Field | Value | Meaning |
|-------|-------|---------|
| Class | ELF64 | 64-bit binary |
| Data | 2's complement, little endian | x86-64 byte order |
| Type | DYN (PIE) | Position-independent executable |
| Machine | x86-64 | AMD/Intel 64-bit |
| Entry point | `0x1340` | `_start` address |
| Program headers | 13 | Segments loaded at runtime |
| Section headers | 31 | Code, data, debug sections |

---

### 3. Section Headers

```bash
objdump -h data_sync
```

**Notable sections:**

| Section | Size | Purpose |
|---------|------|---------|
| `.text` | 0xa34 (2,612 bytes) | Executable machine code |
| `.rodata` | 0x2c7 (711 bytes) | Read-only strings and constants |
| `.dynsym` | 0x300 | Dynamic symbol table |
| `.dynstr` | 0x173 | Dynamic symbol name strings |
| `.plt` | 0x190 | Procedure Linkage Table (imported function stubs) |
| `.got` | 0x100 | Global Offset Table (runtime symbol resolution) |
| `.bss` | 0x30 | Uninitialised global variables |
| `.data` | 0x10 | Initialised global variables |
| `.dynamic` | 0x1f0 | Dynamic linking information |
| `.comment` | 0x2d | Compiler version string |

The `.symtab` and `.strtab` sections are present (binary is **not stripped**), meaning all function names are recoverable with `nm`.

---

### 4. Dynamic Symbol Table

```bash
nm -D data_sync
```

**Imported functions (marked `U` = undefined, resolved at runtime):**

| Symbol | GLIBC Version | Purpose in binary |
|--------|---------------|-------------------|
| `fopen` | 2.2.5 | Open log file, config file, source/destination files |
| `fclose` | 2.2.5 | Close file handles |
| `fread` | 2.2.5 | Read source file contents |
| `fwrite` | 2.2.5 | Write to destination file |
| `fprintf` | 2.2.5 | Write error messages to stderr |
| `fscanf` (`__isoc99_fscanf`) | 2.7 | Parse config file key=value pairs |
| `opendir` | 2.2.5 | Open source directory for traversal |
| `readdir` | 2.2.5 | Iterate directory entries |
| `closedir` | 2.2.5 | Close directory handle |
| `stat` | 2.33 | Check file/directory metadata and modification times |
| `mkdir` | 2.2.5 | Create destination directory |
| `strcmp` | 2.2.5 | Skip `.` and `..` directory entries |
| `strncpy` | 2.2.5 | Copy paths to fixed-size buffers |
| `snprintf` | 2.2.5 | Build path strings |
| `strftime` | 2.2.5 | Format timestamps for log entries |
| `localtime` | 2.2.5 | Convert time_t to local time struct |
| `time` | 2.2.5 | Get current time |
| `strerror` | 2.2.5 | Convert errno to human-readable string |
| `sleep` | 2.2.5 | Pause execution between sync cycles |
| `getopt` | 2.2.5 | Parse command-line arguments (-s, -d, -i, -h) |
| `printf` / `puts` | 2.2.5 | Print status messages to stdout |
| `__stack_chk_fail` | 2.4 | Stack smashing protection (canary check) |
| `__libc_start_main` | 2.34 | C runtime entry point |

**Minimum required GLIBC version:** `2.34` (due to `__libc_start_main@GLIBC_2.34`)

---

### 5. Full Symbol Table

```bash
nm data_sync
```

**Internally defined functions** (binary is not stripped):

| Symbol | Address | Type | Description |
|--------|---------|------|-------------|
| `_start` | `0x1340` | T | C runtime entry point |
| `main` | `0x1b4e` | T | Main function |
| `log_message` | `0x1429` | T | Write timestamped log entry to file |
| `read_config` | `0x1522` | T | Parse `/etc/data_sync.conf` |
| `needs_sync` | `0x160b` | T | Compare mtime of src vs dst |
| `copy_file` | `0x16a9` | T | Copy file contents src → dst |
| `sync_directory` | `0x17e1` | T | Recursively sync a directory |
| `usage` | `0x1ada` | T | Print help message |

---

### 6. Shared Library Dependencies

```bash
ldd data_sync
```

**Output:**
```
linux-vdso.so.1 (0x00007ea032ce6000)
libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007ea032a00000)
/lib64/ld-linux-x86-64.so.2 (0x0000555b18cc4000)
```

**Findings:**
- Only depends on `libc.so.6` — the standard C library
- No third-party or unusual library dependencies
- `linux-vdso.so.1` is a kernel-provided virtual shared object (normal)
- `ld-linux-x86-64.so.2` is the dynamic linker/loader (normal)

This is a minimal, standard C binary with no suspicious external dependencies.

---

### 7. String Extraction

```bash
strings data_sync
```

**Embedded file paths and configuration strings:**

| String | Significance |
|--------|-------------|
| `/var/log/data_sync.log` | Hard-coded log file path (requires write access to `/var/log/`) |
| `/etc/data_sync.conf` | Hard-coded config file path |
| `/home/user/documents` | Default source directory (fallback if no config) |
| `/backup/documents` | Default destination directory (fallback if no config) |
| `source=%s` | Config file format: `source=<path>` |
| `destination=%s` | Config file format: `destination=<path>` |

**Embedded format/log strings:**

| String | Source Function |
|--------|----------------|
| `[ERROR] Cannot open log file: %s` | `log_message` |
| `[WARN] Config file not found, using defaults.` | `read_config` |
| `Cannot open source directory: %s` | `sync_directory` |
| `Synced: %s -> %s` | `sync_directory` |
| `Failed to sync: %s` | `sync_directory` |
| `[data_sync] Starting sync: %s -> %s` | `main` |
| `[data_sync] Running in interval mode every %d seconds.` | `main` |
| `[data_sync] Sync complete.` | `main` |

**Command-line usage strings:**

```
Usage: %s [options]
  -s <src>   Source directory to sync
  -d <dst>   Destination directory
  -i         Run in daemon/interval mode (every 30 seconds)
  -h         Show this help message
```

**Compiler metadata (from `.comment` section):**
```
GCC: (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
```
- Compiled with **GCC 13.3.0** on **Ubuntu 24.04**

---

### 8. Disassembly

```bash
objdump -d data_sync
```

The disassembly confirms the control flow reconstructed from strings and symbols. Key observations:

**`log_message` (`0x1429`)**
- Calls `fopen` to open `/var/log/data_sync.log` in append mode
- If `fopen` fails, calls `strerror` and `fprintf` to stderr
- Calls `time` → `localtime` → `strftime` to generate timestamp
- Calls `fprintf` to write `[timestamp] [level] message` format
- Uses `__stack_chk_fail` — stack canary protection enabled

**`read_config` (`0x1522`)**
- Attempts `fopen("/etc/data_sync.conf", "r")`
- If file missing: uses `strncpy` to set default paths
- If file present: uses `fscanf` with `source=%s` and `destination=%s` patterns

**`needs_sync` (`0x160b`)**
- Calls `stat` on source path; returns `0` if source doesn't exist
- Calls `stat` on destination path; returns `1` if destination doesn't exist
- Compares `st_mtime` fields; returns `1` if source is newer

**`copy_file` (`0x16a9`)**
- Opens source with `fopen(..., "rb")`, destination with `fopen(..., "wb")`
- Reads in 4096-byte chunks (`fread`), writes each chunk (`fwrite`)
- Returns `-1` on any failure, `0` on success

**`sync_directory` (`0x17e1`)**
- Calls `opendir` on source directory
- Calls `mkdir` on destination directory (creates if missing)
- Loops with `readdir`, skipping `.` and `..`
- For each entry: calls `stat`, checks `S_ISDIR` or `S_ISREG`
- Recurses for subdirectories; calls `needs_sync` + `copy_file` for files
- Logs every sync/failure via `log_message`

**`main` (`0x1b4e`)**
- Calls `read_config` first (sets default src/dst)
- Parses args with `getopt` using `"s:d:ih"` option string
- `-s` → `strncpy` to src buffer
- `-d` → `strncpy` to dst buffer
- `-i` → sets `daemon_mode = 1`
- In daemon mode: infinite loop calling `sync_directory` + `sleep(30)`
- In single-run mode: calls `sync_directory` once then exits

---

## Findings Summary

| Property | Value |
|----------|-------|
| Format | ELF 64-bit PIE executable |
| Architecture | x86-64 |
| Compiler | GCC 13.3.0 (Ubuntu 24.04) |
| Stripped | No (all symbols present) |
| External dependencies | `libc.so.6` only |
| Min GLIBC version | 2.34 |
| Stack protection | Yes (`__stack_chk_fail`) |
| Hardcoded paths | `/var/log/data_sync.log`, `/etc/data_sync.conf` |
| Default source | `/home/user/documents` |
| Default destination | `/backup/documents` |
| Daemon capability | Yes (`-i` flag, 30-second interval) |
| Verdict | Legitimate directory sync utility |

---

## Binary Behaviour Reconstruction

Without ever running the binary, analysis reveals the complete behaviour:

1. Reads config from `/etc/data_sync.conf` (format: `source=<path>` / `destination=<path>`)
2. Falls back to `/home/user/documents` → `/backup/documents` if config is missing
3. Command-line flags `-s` and `-d` override config paths
4. Recursively walks the source directory tree
5. For each file: compares modification times with destination
6. Copies only files where source is newer than destination (incremental sync)
7. Creates destination directories as needed
8. Logs all operations with timestamps to `/var/log/data_sync.log`
9. With `-i`: runs the sync loop every 30 seconds indefinitely

---

## Running the Binary

**Single sync run:**
```bash
./data_sync -s /path/to/source -d /path/to/destination
```

**Daemon/interval mode (syncs every 30 seconds):**
```bash
./data_sync -s /path/to/source -d /path/to/destination -i
```

**Show help:**
```bash
./data_sync -h
```

**Using config file** (create `/etc/data_sync.conf`):
```
source=/home/user/documents
destination=/backup/documents
```
Then run:
```bash
./data_sync
```

> **Note:** Logging to `/var/log/data_sync.log` requires write permission to `/var/log/`. Run with `sudo` or change the log path in the source and recompile if needed.

**Recompile from source:**
```bash
gcc -Wall -o data_sync data_sync.c
```