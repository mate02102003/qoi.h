#define QOI_IMPLEMENTATION
#include "../qoi.h"
#include "../thirdparty/flag.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../thirdparty/stb_image.h"

int main(int argc, char **argv) {
    char **input_file = flag_str("input-image", NULL, "Input png image path to convert to qoi (MANDATORY)");
    char **output_file = flag_str("output-image", NULL, "Output qoi image path (MANDATORY)");

    if (!flag_parse(argc, argv)) {
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
