#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>   
#include <pthread.h>   
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>


typedef enum {
    LOG_TRACE = 0,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
} log_level_t;

static const char* LOG_LEVEL_NAMES[] = {
    "TRACE","DEBUG","INFO ","WARN ","ERROR","FATAL"
};
static const char* LOG_LEVEL_COLORS[] = {
    "\x1b[90m", // TRACE
    "\x1b[36m", // DEBUG
    "\x1b[32m", // INFO
    "\x1b[33m", // WARN
    "\x1b[31m", // ERROR
    "\x1b[35m"  // FATAL
};
#define LOG_COLOR_RESET "\x1b[0m"

typedef struct {
    FILE*   console;     
    FILE*   file;       
    int     min_level;   
    int     use_color;   
    pthread_mutex_t lock;
} log_config_t;

static log_config_t g_log = {
    .console   = NULL,
    .file      = NULL,
    .min_level = LOG_INFO,
    .use_color = 0,
    .lock      = PTHREAD_MUTEX_INITIALIZER
};



static char* log_default_path(void) {
    const char* home = getenv("HOME");
    if (!home) home = "."; 

    static char path[512];
    snprintf(path, sizeof path, "%s/.hrc", home);

    mkdir(path, 0755);

    strncat(path, "/hrc.log", sizeof(path) - strlen(path) - 1);

    return path;
}








// ---- API --------------------------------------------------------------------
static inline void log_set_level(log_level_t lvl) { g_log.min_level = (int)lvl; }
static inline void log_set_console(FILE* f)       { g_log.console = f ? f : stdout; g_log.use_color = isatty(fileno(g_log.console)); }
static inline void log_set_color(int enable)      { g_log.use_color = enable; }

static inline int  log_set_file_path(const char* path) {
    if (g_log.file && g_log.file != stdout && g_log.file != stderr) fclose(g_log.file);
    g_log.file = NULL;
    if (!path || !*path) return 0;
    g_log.file = fopen(path, "a");
    if (!g_log.file) return -1;
    return 0;
}
static inline void log_set_file_stream(FILE* f) {
    if (g_log.file && g_log.file != stdout && g_log.file != stderr) fclose(g_log.file);
    g_log.file = f;
}
static inline void log_shutdown(void) {
    if (g_log.file && g_log.file != stdout && g_log.file != stderr) fclose(g_log.file);
    g_log.file = NULL;
}


// if file_path is NULL, uses default path
static inline void log_init(const char* file_path, log_level_t min_level) {
    g_log.console = stdout;
    g_log.use_color = isatty(fileno(g_log.console));
    g_log.min_level = (int)min_level;
    if (file_path && *file_path) {
        if (log_set_file_path(file_path) != 0) {
            perror("logger: fopen");
        }
    }else {
        log_set_file_path(log_default_path());
    }
}

static inline void log__timestamp(char* buf, size_t n) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(buf, n, "%Y-%m-%d %H:%M:%S", &tm);
}

static inline void log__emit(FILE* dest, const char* s, size_t n) {
    if (!dest) return;
    fwrite(s, 1, n, dest);
    fputc('\n', dest);
    fflush(dest);
}

static inline void log_vlog_impl(
    log_level_t level,
    const char* srcfile, int line,
    const char* func,
    const char* fmt, va_list ap_in
) {
    if (level < g_log.min_level) return;
    if (!g_log.console) log_set_console(stdout);

    pthread_mutex_lock(&g_log.lock);

    char ts[20];
    log__timestamp(ts, sizeof ts);

    char msg[2048];
    va_list ap;
    va_copy(ap, ap_in);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    char prefix[256];
    int pn = snprintf(prefix, sizeof prefix, "%s [%s] %s:%d (%s): ",
                      ts, LOG_LEVEL_NAMES[level], srcfile, line, func);
    if (pn < 0) pn = 0;

    char linebuf[4096];
    size_t used = 0;
    if (g_log.use_color && g_log.console) {
        int n = snprintf(linebuf, sizeof linebuf, "%s%s%s %s",
                         LOG_LEVEL_COLORS[level], LOG_LEVEL_NAMES[level], LOG_COLOR_RESET, msg);
        if (n < 0) n = 0;
        used = (size_t)n;
    } else {
        int n = snprintf(linebuf, sizeof linebuf, "%s%s", prefix, msg);
        if (n < 0) n = 0;
        used = (size_t)n;
    }

    log__emit(g_log.console, linebuf, used);

    if (g_log.file) {
        char fileline[4096];
        int n = snprintf(fileline, sizeof fileline, "%s%s", prefix, msg);
        if (n < 0) n = 0;
        log__emit(g_log.file, fileline, (size_t)n);
    }

    if (level == LOG_FATAL) {
        pthread_mutex_unlock(&g_log.lock);
        log_shutdown();
        abort();
    }

    pthread_mutex_unlock(&g_log.lock);
}

static inline void log_log_impl(
    log_level_t level,
    const char* srcfile, int line,
    const char* func,
    const char* fmt, ...
) {
    va_list ap;
    va_start(ap, fmt);
    log_vlog_impl(level, srcfile, line, func, fmt, ap);
    va_end(ap);
}


#define LOG_TRACE(...) log_log_impl(LOG_TRACE, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_DEBUG(...) log_log_impl(LOG_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_INFO(...)  log_log_impl(LOG_INFO,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_WARN(...)  log_log_impl(LOG_WARN,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_ERROR(...) log_log_impl(LOG_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_FATAL(...) log_log_impl(LOG_FATAL, __FILE__, __LINE__, __func__, __VA_ARGS__)

#endif
