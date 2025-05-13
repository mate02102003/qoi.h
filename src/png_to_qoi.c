#define QOI_IMPLEMENTATION
#include "../qoi.h"
#define FLAG_IMPLEMENTATION
#include "../thirdparty/flag.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../thirdparty/stb_image.h"

void usage(FILE *stream)
{
    fprintf(stream, "Usage: ./png_to_qoi [OPTIONS]\n");
    fprintf(stream, "OPTIONS:\n");
    flag_print_options(stream);
}

int main(int argc, char **argv) {
    bool *help = flag_bool("help", false, "Print this help to stdout and exit with 0");
    char **input_file = flag_str("input-image", NULL, "Input png image path to convert to qoi (MANDATORY)");
    char **output_file = flag_str("output-image", NULL, "Output qoi image path (MANDATORY)");

    if (!flag_parse(argc, argv)) {
        usage(stderr);
        flag_print_error(stderr);
        return 1;
    }

    if (*help) {
        usage(stdout);
        exit(0);
    }

    if (*input_file == NULL) {
        usage(stderr);
        fprintf(stderr, "ERROR: No -%s was provided\n", flag_name(input_file));
        return 1;
    }
    if (*output_file == NULL) {
        usage(stderr);
        fprintf(stderr, "ERROR: No -%s was provided\n", flag_name(output_file));
        return 1;
    }

    int w, h, comp;
    void *data = stbi_load(*input_file, &w, &h, &comp, 4);
    if (data == NULL) {
        return 2;
    }

    if(!qoi_write_image(*output_file, w, h, comp, 1, data)) {
        return 3;
    }

    stbi_image_free(data);
    return 0;
}
