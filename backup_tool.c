#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#define LOG_FILE   "backup.log"
#define BACKUP_DIR "backup_output"
#define SOURCE_FILE "source_data.txt"

/* Write a timestamped entry to the log file */
void write_log(FILE *log_fp, const char *level, const char *message) {
    time_t now = time(NULL);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(log_fp, "[%s] [%s] %s\n", ts, level, message);
    fflush(log_fp);
}

/* Create the source file with sample data to back up */
void create_source_file(void) {
    FILE *fp = fopen(SOURCE_FILE, "w");
    if (!fp) {
        perror("fopen source");
        exit(EXIT_FAILURE);
    }
    fprintf(fp, "Record 1: server01 uptime=99.9%%\n");
    fprintf(fp, "Record 2: server02 uptime=98.7%%\n");
    fprintf(fp, "Record 3: server03 uptime=97.2%%\n");
    fprintf(fp, "Record 4: server04 uptime=99.1%%\n");
    fprintf(fp, "Record 5: server05 uptime=100.0%%\n");
    fclose(fp);
    printf("[backup_tool] Created source file: %s\n", SOURCE_FILE);
}

/* Read source file and return allocated buffer — caller must free */
char *read_source_file(size_t *out_len) {
    FILE *fp = fopen(SOURCE_FILE, "r");
    if (!fp) {
        perror("fopen read");
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buf = malloc(len + 1);
    if (!buf) { fclose(fp); return NULL; }

    fread(buf, 1, len, fp);
    buf[len] = '\0';
    fclose(fp);
    *out_len = (size_t)len;
    return buf;
}

/* Back up data: write content into backup_output/backup_<timestamp>.bak */
int perform_backup(const char *data, size_t len, FILE *log_fp) {
    /* Create backup directory if needed */
    if (mkdir(BACKUP_DIR, 0755) != 0 && errno != EEXIST) {
        perror("mkdir");
        return -1;
    }

    /* Build timestamped filename */
    time_t now = time(NULL);
    char fname[256];
    struct tm *t = localtime(&now);
    snprintf(fname, sizeof(fname), "%s/backup_%04d%02d%02d_%02d%02d%02d.bak",
             BACKUP_DIR,
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);

    /* Open backup file and write */
    int fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open backup");
        return -1;
    }
    ssize_t written = write(fd, data, len);
    if (written < 0) perror("write backup");
    close(fd);

    char msg[512];
    snprintf(msg, sizeof(msg), "Backup written to %s (%zd bytes)", fname, written);
    write_log(log_fp, "INFO", msg);
    printf("[backup_tool] %s\n", msg);
    return 0;
}

/* Verify backup: stat the output dir and list count */
void verify_backup(FILE *log_fp) {
    struct stat st;
    if (stat(BACKUP_DIR, &st) == 0) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "Backup directory verified: %s (mode=%o)", BACKUP_DIR, st.st_mode & 0777);
        write_log(log_fp, "INFO", msg);
        printf("[backup_tool] %s\n", msg);
    } else {
        write_log(log_fp, "ERROR", "Backup directory verification failed");
    }
}

int main(int argc, char *argv[]) {
    printf("[backup_tool] Starting backup process. PID=%d\n", getpid());

    /* Open log file */
    FILE *log_fp = fopen(LOG_FILE, "a");
    if (!log_fp) { perror("fopen log"); return 1; }

    write_log(log_fp, "INFO", "backup_tool started");

    /* Step 1: create source file */
    create_source_file();
    write_log(log_fp, "INFO", "Source file created");

    /* Step 2: read source file */
    size_t data_len = 0;
    char *data = read_source_file(&data_len);
    if (!data) {
        write_log(log_fp, "ERROR", "Failed to read source file");
        fclose(log_fp);
        return 1;
    }
    char msg[128];
    snprintf(msg, sizeof(msg), "Read %zu bytes from %s", data_len, SOURCE_FILE);
    write_log(log_fp, "INFO", msg);
    printf("[backup_tool] %s\n", msg);

    /* Step 3: perform backup */
    if (perform_backup(data, data_len, log_fp) != 0) {
        write_log(log_fp, "ERROR", "Backup failed");
        free(data);
        fclose(log_fp);
        return 1;
    }

    /* Step 4: verify */
    verify_backup(log_fp);

    free(data);
    write_log(log_fp, "INFO", "backup_tool completed successfully");
    fclose(log_fp);
    printf("[backup_tool] Done.\n");
    return 0;
}
