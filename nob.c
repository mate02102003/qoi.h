#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

#define BUILD_FOLDER "/build"
#define SOURCE_FOLDER "/src"

bool build_target_sync(Cmd *cmd, const char *target, const char *output) {
    cmd_append(cmd, "cc");
    cmd_append(cmd, target);
    cmd_append(cmd, "-o", output);
    cmd_append(cmd, "-Wall", "-Wextra");
    cmd_append(cmd, "-O3");

    if (!cmd_run_sync_and_reset(cmd)) return false;

    return 0;
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    Cmd cmd = {0};

    if (!nob_mkdir_if_not_exists(BUILD_FOLDER)) return 1;

    if (!build_target_sync(&cmd, SOURCE_FOLDER"qoi_to_png.c", BUILD_FOLDER"qoi_to_png")) return 1;
    if (!build_target_sync(&cmd, SOURCE_FOLDER"png_to_qoi.c", BUILD_FOLDER"png_to_qoi")) return 1;

    return 0;
}