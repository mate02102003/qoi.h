#define QOI_IMPLEMENTATION
#include "../qoi.h"
#include "../thirdparty/flag.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../thirdparty/stb_image_write.h"

int main(int argc, char **argv) {
    char **input_file = flag_str("input-image", NULL, "Input qoi image path to convert to png (MANDATORY)");
    char **output_file = flag_str("output-image", NULL, "Output png image path (MANDATORY)");

    if (!flag_parse(argc, argv)) {
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
