#define NOB_IMPLEMENTATION
#include "thirdparty/nob.h"
#define FLAG_IMPLEMENTATION
#include "thirdparty/flag.h"

#define BUILD_FOLDER "build/"
#define SOURCE_FOLDER "src/"

void usage(FILE *stream)
{
    fprintf(stream, "Usage: ./nob [OPTIONS]\n");
    fprintf(stream, "OPTIONS:\n");
    flag_print_options(stream);
}

bool build_target_sync_and_reset(Nob_Cmd *cmd, const char *target, const char *output, bool optimize) {
    nob_cmd_append(cmd, "cc");
    nob_cmd_append(cmd, target);
    nob_cmd_append(cmd, "-o", output);
    nob_cmd_append(cmd, "-Wall", "-Wextra");
    if (optimize) nob_cmd_append(cmd, "-O3");
    nob_cmd_append(cmd, "-lm");

    if (!nob_cmd_run_sync_and_reset(cmd)) return false;
    
    return true;
}

char *python__version(char *version) {
#ifndef _WIN32
    return version;
#else
    char *major = version, *minor;

    while (*version++ != '.');
    *(version - 1) = '\0';
    minor = version;

    return nob_temp_sprintf("%s%s", major, minor);
#endif
}

bool build_python_library_sync_and_reset(Nob_Cmd *cmd, char *version, const char *python_include_path, const char* python_library_path, bool optimize) {
    nob_cmd_append(cmd, "cc");
    nob_cmd_append(cmd, "-fPIC");
    nob_cmd_append(cmd, "-shared");
    nob_cmd_append(cmd, SOURCE_FOLDER"qoipy.c");
#ifndef _WIN32
    nob_cmd_append(cmd, "-o", BUILD_FOLDER"qoipy.so");
#else
    nob_cmd_append(cmd, "-o", BUILD_FOLDER"qoipy.pyd");
#endif
    nob_cmd_append(cmd, "-Wall", "-Wextra");
    if (optimize) nob_cmd_append(cmd, "-O3");
    nob_cmd_append(cmd, nob_temp_sprintf("-I%s", python_include_path));
#ifdef _WIN32
    nob_cmd_append(cmd, nob_temp_sprintf("-L%s", python_library_path));
#endif
    nob_cmd_append(cmd, "-lpython3");
    nob_cmd_append(cmd, nob_temp_sprintf("-lpython%s", version));

    if (!nob_cmd_run_sync_and_reset(cmd)) return false;

    return true;
}

int main(int argc, char **argv) {
    bool *help            = flag_bool("help",  false, "Print this help to stdout and exit with 0");
    bool *optimize        = flag_bool("O",     false, "Enable optimisation");
    char **python_version = flag_str ("PYVer", NULL,  "[MANDATORY] Specifiy Python version (Usage: -PYVer 3.13)");

    int    flag_argc = argc;
    char **flag_argv = argv; 
    NOB_GO_REBUILD_URSELF(argc, argv);
    
    if (!flag_parse(flag_argc, flag_argv)) {
        usage(stderr);
        flag_print_error(stderr);
        return 1;
    }    
    if (*help) {
        usage(stderr);
        return 0;
    }

    if (*python_version == NULL) {
        usage(stderr);
        return 1;
    }

    *python_version = python__version(*python_version);

    Nob_Cmd cmd = {0};

    if (!nob_mkdir_if_not_exists(BUILD_FOLDER)) return 1;

    if (!build_target_sync_and_reset(&cmd, SOURCE_FOLDER"qoi_to_png.c", BUILD_FOLDER"qoi_to_png", *optimize)) return 1;
    if (!build_target_sync_and_reset(&cmd, SOURCE_FOLDER"png_to_qoi.c", BUILD_FOLDER"png_to_qoi", *optimize)) return 1;
#ifndef _WIN32
    if (!build_python_library_sync_and_reset(&cmd, *python_version, nob_temp_sprintf("/usr/include/python%s", *python_version), NULL, *optimize)) return 1;
#else
    char *local = getenv("LOCALAPPDATA");
    if (local == NULL) return 1;

    char *python_path = nob_temp_sprintf("%s\\Programs\\Python\\Python%s", local, *python_version);
    if (!build_python_library_sync_and_reset(&cmd, *python_version, nob_temp_sprintf("%s\\include", python_path), nob_temp_sprintf("%s\\libs", python_path), *optimize)) return 1;
#endif
    if (!nob_copy_file(SOURCE_FOLDER"qoipy.pyi", BUILD_FOLDER"qoipy.pyi")) return 1;
    
    return 0;
}