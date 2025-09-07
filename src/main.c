#include "../include/logger.h"



int main(void) {
    log_init(NULL, LOG_TRACE);


    LOG_INFO("Hot Reload Started\nWatching for changes...");

    log_shutdown();
    return 0;
}
