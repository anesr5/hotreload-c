#include <string.h>
#include <stdlib.h>
#include "logger.h"
#include "hot_reload.h"

static void usage(const char* prog, int code) {
    LOG_ERROR("invalid or missing flags");
    LOG_INFO("Usage:");
    LOG_INFO("  %s -s  -cmd \"<command>\"",  prog);
    LOG_INFO("  %s -sl -cmd \"<command>\"",  prog);
    LOG_INFO("Options:");
    LOG_INFO("  -s       Stateful mode (keep/restart child on changes)");
    LOG_INFO("  -sl      Stateless mode (re-run cmd on changes)");
    LOG_INFO("  -cmd     Command to run on changes (quoted string)");
    exit(code);
}
