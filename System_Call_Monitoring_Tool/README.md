# System Call Monitoring Tool

A C program that simulates a backup tool using low-level system calls. It creates a source data file, reads it, writes a timestamped backup, and verifies the backup directory — logging every step.

## Files

| File | Description |
|------|-------------|
| `backup_tool.c` | Main source code |
| `strace_output.txt` | Sample strace output for system call analysis |
| `backup.log` | Generated log file (created at runtime) |
| `source_data.txt` | Generated source data file (created at runtime) |
| `backup_output/` | Directory containing backup `.bak` files (created at runtime) |

## Requirements

- GCC compiler
- macOS or Linux

## Compilation

```bash
gcc -Wall -o backup_tool backup_tool.c
```

## Usage

```bash
./backup_tool
```

## What It Does

1. **Creates** `source_data.txt` with sample server uptime records
2. **Reads** the source file into memory
3. **Backs up** the data to `backup_output/backup_<timestamp>.bak`
4. **Verifies** the backup directory exists and checks its permissions
5. **Logs** every step with timestamps to `backup.log`

## Expected Output

```
[backup_tool] Starting backup process. PID=XXXXX
[backup_tool] Created source file: source_data.txt
[backup_tool] Read 161 bytes from source_data.txt
[backup_tool] Backup written to backup_output/backup_YYYYMMDD_HHMMSS.bak (161 bytes)
[backup_tool] Backup directory verified: backup_output (mode=755)
[backup_tool] Done.
```

## System Calls Used

| System Call | Purpose |
|-------------|---------|
| `open()` | Open backup file for writing |
| `write()` | Write backup data to file |
| `close()` | Close file descriptor |
| `mkdir()` | Create backup output directory |
| `stat()` | Verify backup directory and get permissions |
| `fopen/fread/fwrite` | File I/O for source and log files |
| `getpid()` | Retrieve process ID |
| `time()` | Generate timestamps for filenames and logs |