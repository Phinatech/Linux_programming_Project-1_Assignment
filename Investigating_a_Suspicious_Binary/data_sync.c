#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#define MAX_PATH 1024
#define LOG_FILE "/var/log/data_sync.log"
#define CONFIG_FILE "/etc/data_sync.conf"
#define SYNC_INTERVAL 30

/* Log a message with timestamp to the log file */
void log_message(const char *level, const char *message) {
    FILE *log_fp = fopen(LOG_FILE, "a");
    if (!log_fp) {
        fprintf(stderr, "[ERROR] Cannot open log file: %s\n", strerror(errno));
        return;
    }
    time_t now = time(NULL);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(log_fp, "[%s] [%s] %s\n", time_buf, level, message);
    fclose(log_fp);
}

/* Read sync paths from config file */
int read_config(char *src, char *dst) {
    FILE *cfg = fopen(CONFIG_FILE, "r");
    if (!cfg) {
        fprintf(stderr, "[WARN] Config file not found, using defaults.\n");
        strncpy(src, "/home/user/documents", MAX_PATH);
        strncpy(dst, "/backup/documents", MAX_PATH);
        return 0;
    }
    fscanf(cfg, "source=%s\n", src);
    fscanf(cfg, "destination=%s\n", dst);
    fclose(cfg);
    return 1;
}

/* Check if a file needs syncing based on modification time */
int needs_sync(const char *src_path, const char *dst_path) {
    struct stat src_stat, dst_stat;
    if (stat(src_path, &src_stat) != 0) return 0;
    if (stat(dst_path, &dst_stat) != 0) return 1; /* Destination doesn't exist */
    return src_stat.st_mtime > dst_stat.st_mtime;
}

/* Copy a single file from src to dst */
int copy_file(const char *src_path, const char *dst_path) {
    FILE *src_fp = fopen(src_path, "rb");
    if (!src_fp) return -1;

    FILE *dst_fp = fopen(dst_path, "wb");
    if (!dst_fp) {
        fclose(src_fp);
        return -1;
    }

    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src_fp)) > 0) {
        fwrite(buffer, 1, bytes, dst_fp);
    }
    fclose(src_fp);
    fclose(dst_fp);
    return 0;
}

/* Synchronize a directory recursively */
void sync_directory(const char *src_dir, const char *dst_dir) {
    DIR *dir = opendir(src_dir);
    if (!dir) {
        char msg[MAX_PATH * 2];
        snprintf(msg, sizeof(msg), "Cannot open source directory: %s", src_dir);
        log_message("ERROR", msg);
        return;
    }

    mkdir(dst_dir, 0755);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char src_path[MAX_PATH], dst_path[MAX_PATH];
        snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, entry->d_name);

        struct stat st;
        if (stat(src_path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            sync_directory(src_path, dst_path);
        } else if (S_ISREG(st.st_mode)) {
            if (needs_sync(src_path, dst_path)) {
                if (copy_file(src_path, dst_path) == 0) {
                    char msg[MAX_PATH * 2];
                    snprintf(msg, sizeof(msg), "Synced: %s -> %s", src_path, dst_path);
                    log_message("INFO", msg);
                } else {
                    char msg[MAX_PATH * 2];
                    snprintf(msg, sizeof(msg), "Failed to sync: %s", src_path);
                    log_message("ERROR", msg);
                }
            }
        }
    }
    closedir(dir);
}

/* Print usage information */
void usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("  -s <src>   Source directory to sync\n");
    printf("  -d <dst>   Destination directory\n");
    printf("  -i         Run in daemon/interval mode (every %d seconds)\n", SYNC_INTERVAL);
    printf("  -h         Show this help message\n");
}

int main(int argc, char *argv[]) {
    char src[MAX_PATH], dst[MAX_PATH];
    int daemon_mode = 0;
    int opt;

    read_config(src, dst);

    while ((opt = getopt(argc, argv, "s:d:ih")) != -1) {
        switch (opt) {
            case 's': strncpy(src, optarg, MAX_PATH); break;
            case 'd': strncpy(dst, optarg, MAX_PATH); break;
            case 'i': daemon_mode = 1; break;
            case 'h': usage(argv[0]); return 0;
            default:  usage(argv[0]); return 1;
        }
    }

    printf("[data_sync] Starting sync: %s -> %s\n", src, dst);
    log_message("INFO", "data_sync started");

    if (daemon_mode) {
        printf("[data_sync] Running in interval mode every %d seconds.\n", SYNC_INTERVAL);
        while (1) {
            sync_directory(src, dst);
            sleep(SYNC_INTERVAL);
        }
    } else {
        sync_directory(src, dst);
        log_message("INFO", "data_sync completed");
        printf("[data_sync] Sync complete.\n");
    }

    return 0;
}
