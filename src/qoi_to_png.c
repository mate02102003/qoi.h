#define QOI_IMPLEMENTATION
#include "../qoi.h"
#define FLAG_IMPLEMENTATION
#include "../thirdparty/flag.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../thirdparty/stb_image_write.h"

void usage(FILE *stream)
{
    fprintf(stream, "Usage: ./qoi_to_png [OPTIONS]\n");
    fprintf(stream, "OPTIONS:\n");
    flag_print_options(stream);
}

int main(int argc, char **argv) {
    bool *help = flag_bool("help", false, "Print this help to stdout and exit with 0");
    char **input_file = flag_str("input-image", NULL, "Input qoi image path to convert to png (MANDATORY)");
    char **output_file = flag_str("output-image", NULL, "Output png image path (MANDATORY)");

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

    qoi_image image = {0};
    if (!qoi_load_image(*input_file, &image)) {
        return 2;
    }

    if (!stbi_write_png(*output_file, image.header.width, image.header.height, 4, image.image_data.items, 0)) {
        return 3;
    }

    qoi_free_image(&image);
    return 0;
}
