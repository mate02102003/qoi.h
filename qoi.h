#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#ifndef QOI_H_
#define QOI_H_

#define QOI_MAGIC "qoif"
#define QOI_HEADER_SIZE 14
#define QOI_END_SIZE 8
#define QOI_END (uint8_t[]) { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }

#ifndef QOI_DA_INIT_CAP
#define QOI_DA_INIT_CAP 256
#endif

#define qoi_da_append(da, item)                                                          \
    do {                                                                                 \
        if ((da)->count >= (da)->capacity) {                                             \
            (da)->capacity = (da)->capacity == 0 ? QOI_DA_INIT_CAP : (da)->capacity*2;   \
            (da)->items = realloc((da)->items, (da)->capacity*sizeof(*(da)->items));     \
        }                                                                                \
                                                                                         \
        (da)->items[(da)->count++] = (item);                                             \
    } while (0)

#define qoi_write_image_from_header(filepath, header, pixels) qoi_write_image(filepath, header.width, header.height, header.chanels, header.colorspace, pixels)

typedef enum {
    INDEX = 0b00000000,
    DIFF  = 0b01000000,
    LUMA  = 0b10000000,
    RUN   = 0b11000000,
    RGB   = 0b11111110,
    RGBA  = 0b11111111,
} qoi_tags_mask;

typedef struct {
    char     magic[4];
    uint32_t width;
    uint32_t height;
    uint8_t  chanels;
    uint8_t  colorspace;
} qoi_header;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} qoi_rgba;

typedef qoi_rgba Pixel;
typedef struct {
    uint32_t count;
    uint32_t capacity;
    Pixel    *items;
} Pixels;

typedef struct {
    qoi_header header;
    Pixels     image_data;
} qoi_image;

uint8_t qoi_hash(qoi_rgba *color);
void *qoi_change_byte_order(void *b, const size_t size);
int qoi_load_image_header(int fd, qoi_image *image);
int qoi_load_image_data(int fd, qoi_image *image);
int qoi_load_image(const char *filepath, qoi_image *image);
void qoi_free_image(qoi_image *image);
int qoi_write_image_from_qoi(const char *filepath, qoi_image image);
int qoi_write_image(const char *filepath, uint32_t width, uint32_t height, uint8_t chanels, uint8_t colorspace, Pixel *pixels);

#endif // QOI_HEADER
#ifdef QOI_IMPLEMENTATION

inline uint8_t qoi_hash(qoi_rgba *color) {
    return (color->r * 3 + color->g * 5 + color->b * 7 + color->a * 11) % 64;
}

void *qoi_change_byte_order(void *b, const size_t size) {
    assert(size % 2 == 0);
    uint8_t *t = (uint8_t *)b;
    for (size_t i = 0; i < size / 2; ++i) {
        uint8_t temp = t[i];
        t[i] = t[size - i - 1];
        t[size - i - 1] = temp;
    }
    return b;
}

int qoi_load_image_header(int fd, qoi_image* image) {
    if (read(fd, &image->header, QOI_HEADER_SIZE) != QOI_HEADER_SIZE) {
        return -1;
    }

    if (memcmp(image->header.magic, QOI_MAGIC, 4) != 0) {
        return -1;
    }

    qoi_change_byte_order(&image->header.width, sizeof(image->header.width));
    qoi_change_byte_order(&image->header.height, sizeof(image->header.height));

    return 0;
}

int qoi_load_image_data(int fd, qoi_image* image) {
    const long data_size = lseek(fd, 0, SEEK_END) - QOI_HEADER_SIZE - QOI_END_SIZE;
    uint8_t* data = (uint8_t*)malloc(data_size+1);
    memset(data, 0, data_size+1);

    lseek(fd, QOI_HEADER_SIZE, SEEK_SET);
    int data_read_count = read(fd, data, data_size);
    if (data_read_count < data_size) {
        return -1;
    }

    uint8_t end[QOI_END_SIZE];
    int end_read_count = read(fd, end, QOI_END_SIZE);
    if (QOI_END_SIZE > end_read_count) {
        return -1;
    }
    if (memcmp(QOI_END, end, QOI_END_SIZE)) {
        return -1;
    }

    Pixel lookup_array[64] = {0};
    Pixel cur_px  = { 0, 0, 0, 255 };
    Pixel prev_px = cur_px;

    for (;image->image_data.count < image->header.width * image->header.height; data++) {
        lookup_array[qoi_hash(&prev_px)] = prev_px;

        if (*data == RGBA) {
            cur_px.r = *++data;
            cur_px.g = *++data;
            cur_px.b = *++data;
            cur_px.a = *++data;
        }
        else if (*data == RGB) {
            cur_px.r = *++data;
            cur_px.g = *++data;
            cur_px.b = *++data;
        }
        else if ((*data & 0b11000000) == INDEX) {
            uint8_t index = 0b00111111 & *data;
            cur_px = lookup_array[index];
        }
        else if ((*data & 0b11000000) == DIFF) {
            cur_px.r = prev_px.r + ((*data >> 4) & 0b00000011) - 2;
            cur_px.g = prev_px.g + ((*data >> 2) & 0b00000011) - 2;
            cur_px.b = prev_px.b + (*data & 0b00000011) - 2;
        }
        else if ((*data & 0b11000000) == LUMA) {
            int8_t dg = (*data++ & 0b00111111) - 32;
            int8_t dr_dg = (*data >> 4 & 0b00001111) - 8;
            int8_t db_dg = (*data & 0b00001111) - 8;

            cur_px.r = prev_px.r + dr_dg + dg;
            cur_px.g = prev_px.g + dg;
            cur_px.b = prev_px.b + db_dg + dg;
        }
        else if ((*data & 0b11000000) == RUN) {
            uint8_t times = (*data & 0b00111111);
            for(uint8_t repeat = 0; repeat <= times; ++repeat) {
                qoi_da_append(&image->image_data, prev_px);
            }
            continue;
        }
        
        qoi_da_append(&image->image_data, cur_px);
        prev_px = cur_px;
    }

    return 0;
}

int qoi_load_image(const char* filepath, qoi_image* image) {
    int fd = open(filepath, O_RDONLY);

    if (-1 == fd) {
        fprintf(stderr, "[ERROR]: Couldn't open file!\n");
        goto error;
    }
    if (-1 == qoi_load_image_header(fd, image)) {
        fprintf(stderr, "[ERROR]: Incorrect header data!\n");
        goto error;
    }
    if (-1 == qoi_load_image_data(fd, image)) {
        fprintf(stderr, "[ERROR]: Incorrect image data!\n");
        goto error;
    }
    if (image->header.width * image->header.height != image->image_data.count) {
        fprintf(stderr, "[ERROR]: Image width (%u) and heigth (%u) doesn't match parsed pixel count (%u)!\n", image->header.width, image->header.height, image->image_data.count);
        goto error;
    }
    
    close(fd);
    return 0;

error:
    close(fd);
    return -1;
}

void qoi_free_image(qoi_image* image) {
    free(image->image_data.items);
}

int qoi_write_image_from_qoi(const char* filepath, qoi_image image){
    return qoi_write_image_from_header(filepath, image.header, image.image_data.items);
}

int qoi_write_image(const char* filepath, uint32_t width, uint32_t height, uint8_t chanels, uint8_t colorspace, Pixel* pixels) {
    int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC);

    if (-1 == fd) {
        fprintf(stderr, "[ERROR]: %s: %s\n", "Couldn't open file", filepath);
        goto error;
    }

    write(fd, QOI_MAGIC, strlen(QOI_MAGIC));
    write(fd, qoi_change_byte_order(&width, sizeof(width)), sizeof(width));
    write(fd, qoi_change_byte_order(&height, sizeof(height)), sizeof(height));
    write(fd, &chanels, sizeof(chanels));
    write(fd, &colorspace, sizeof(colorspace));

    Pixel lookup_array[64] = {0};
    Pixel prev_px = { 0, 0, 0, 255 };
    uint8_t same_pixel = RUN;

    uint8_t type;
    for (size_t i = 0; i < width * height; i++, pixels++) {
        uint8_t hash = qoi_hash(pixels);
        lookup_array[hash] = prev_px;

        // RUN
        type = RUN;
        if (same_pixel == RGB) {
            same_pixel--;
            write(fd, &same_pixel, 1);
            same_pixel = RUN + 1;
        }
        if (memcmp(pixels, pixels, sizeof(Pixel))) {
            same_pixel++;
            continue;
        }
        else {
            write(fd, &same_pixel, 1);
            same_pixel = RUN; 
        }

        int8_t dr = pixels->r - prev_px.r;
        int8_t dg = pixels->g - prev_px.g;
        int8_t db = pixels->b - prev_px.b;
        int8_t dr_dg = dr - dg;
        int8_t dr_db = dr - db;

        // INDEX
        if (memcmp(&lookup_array[hash], pixels, sizeof(Pixel))) {
            type = INDEX;
            write(fd, &hash, 1);
        }
        // DIFF
        else if (-2 <= dr && dr <= 1 && -2 <= dg && dg <= 1 && -2 <= db && db <= 1) {
            type = DIFF;
            uint8_t diff = DIFF & (dr + 2) << 4 & (dg + 2) << 2 & (db + 2);
            write(fd, &diff, 1);
        }
        // LUMA
        else if (-32 <= dr && dr <= 31 && -8 <= dr_dg && dr_dg <= 7 && -8 <= dr_db && dr_db <= 7) {
            type = LUMA;
            uint8_t luma[2] = { LUMA & (dr + 32), dr_dg << 4 & dr_db };
            write(fd, luma, 2);
        }
        // RGB
        else if (chanels == 3) {
            type = RGB;
            write(fd, &type, 1);
            write(fd, pixels, 3);
        }
        // RGBA
        else {
            type = RGBA;
            write(fd, &type, 1);
            write(fd, pixels, 4);
        }

        prev_px = *pixels;
    }

    write(fd, QOI_END, QOI_END_SIZE);
    close(fd);
    return 0;
error:
    close(fd);
    return -1;
}

#endif // QOI_IMPLEMENTATION