#include "../include/logger.h"
#include "../include/utils.h"
#include "../include/hot_reload.h"
#include "../include/build.h"  

int main(int argc, char* argv[]) {
    log_init(NULL, LOG_INFO);

    hr_mode_t mode = (hr_mode_t)(-1);
    const char* cmd = NULL;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-s") == 0) {
            if (mode != (hr_mode_t)(-1)) usage(argv[0], EXIT_FAILURE);
            mode = HR_STATEFUL;
        } else if (strcmp(argv[i], "-sl") == 0) {
            if (mode != (hr_mode_t)(-1)) usage(argv[0], EXIT_FAILURE);
            mode = HR_STATELESS;
        } else if (strcmp(argv[i], "-cmd") == 0) {
            if (i + 1 >= argc) usage(argv[0], EXIT_FAILURE);
            cmd = argv[++i];
        } else {
            LOG_WARN("unknown argument: %s", argv[i]);
            usage(argv[0], EXIT_FAILURE);
        }
    }

    if (mode == (hr_mode_t)(-1) || !cmd) {
        usage(argv[0], EXIT_FAILURE);
    }

    LOG_INFO("mode: %s", mode == HR_STATEFUL ? "STATEFUL (-s)" : "STATELESS (-sl)");
    LOG_INFO("cmd:  %s", cmd);

    build_spec_t spec;
    if (parse_user_cmd(cmd, &spec) != 0) {
        LOG_ERROR("échec du parsing de la commande -cmd");
        log_shutdown();
        return EXIT_FAILURE;
    }

    LOG_INFO("compiler: %s", spec.compiler);
    LOG_INFO("sources  : %d fichiers .c", spec.cfiles_n);
    for (int i = 0; i < spec.cfiles_n; ++i) LOG_DEBUG("  cfile[%d]: %s", i, spec.cfiles[i]);
    LOG_INFO("cflags   : %d", spec.cflags_n);
    LOG_INFO("ldflags  : %d", spec.ldflags_n);


    free_build_spec(&spec);
    log_shutdown();
    return 0;
}
