#define NOB_IMPLEMENTATION
#include "thirdparty/nob.h"

#define BUILD_FOLDER "build/"
#define SOURCE_FOLDER "src/"

bool build_target_sync(Nob_Cmd *cmd, const char *target, const char *output) {
    nob_cmd_append(cmd, "cc");
    nob_cmd_append(cmd, target);
    nob_cmd_append(cmd, "-o", output);
    nob_cmd_append(cmd, "-Wall", "-Wextra");
    nob_cmd_append(cmd, "-O3");
    nob_cmd_append(cmd, "-lm");

    if (!nob_cmd_run_sync_and_reset(cmd)) return false;

    return true;
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    Nob_Cmd cmd = {0};

    if (!nob_mkdir_if_not_exists(BUILD_FOLDER)) return 1;

    if (!build_target_sync(&cmd, SOURCE_FOLDER"qoi_to_png.c", BUILD_FOLDER"qoi_to_png")) return 1;
    if (!build_target_sync(&cmd, SOURCE_FOLDER"png_to_qoi.c", BUILD_FOLDER"png_to_qoi")) return 1;

    return 0;
}