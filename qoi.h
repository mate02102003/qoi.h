#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifndef QOI_H_
#define QOI_H_

#define QOI_MAGIC "qoif"
#define QOI_HEADER_SIZE 14
#define QOI_END_SIZE 8
#define QOI_END (uint8_t[]) { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }

#ifndef QOI_DA_INIT_CAP
#define QOI_DA_INIT_CAP 65536U
#endif

#define qoi_da_append(da, item)                                                          \
    do {                                                                                 \
        if ((da)->count >= (da)->capacity) {                                             \
            (da)->capacity = (da)->capacity == 0 ? QOI_DA_INIT_CAP : (da)->capacity*2;   \
            (da)->items = realloc((da)->items, (da)->capacity*sizeof(*(da)->items));     \
        }                                                                                \
        (da)->items[(da)->count++] = (item);                                             \
    } while (0)

#define qoi_write_image_from_header(filepath, header, pixels) qoi_write_image(filepath, header.width, header.height, header.chanels, header.colorspace, pixels)
#define qoi_write_image_from_qoi_image(filepath, image) qoi_write_image_from_header(filepath, image.header, image.image_data.items)

#define qoi_between(n, l, h) l <= n && n <= h // ends included

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

typedef struct {
    uint32_t count;
    uint32_t capacity;
    qoi_rgba *items;
} qoi_rgbas;

typedef struct {
    qoi_header header;
    qoi_rgbas  image_data;
} qoi_image;

uint8_t qoi_hash(qoi_rgba *color);
void *qoi_change_byte_order(void *b, const size_t size);
bool qoi_load_image_header(FILE *fd, qoi_image *image);
bool qoi_load_image_data(FILE *fd, qoi_image *image);
bool qoi_load_image(const char *filepath, qoi_image *image);
void qoi_free_image(qoi_image *image);
bool qoi_write_image(const char *filepath, uint32_t width, uint32_t height, uint8_t chanels, uint8_t colorspace, qoi_rgba *pixels);

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

bool qoi_load_image_header(FILE *fd, qoi_image* image) {
    if (fread(&image->header, QOI_HEADER_SIZE, 1, fd) != 1) {
        fprintf(stderr, "[ERROR]: Incorrect header read size!\n");
        return false;
    }
    
    if (memcmp(image->header.magic, QOI_MAGIC, 4) != 0) {
        fprintf(stderr, "[ERROR]: Incorrect magic!\n");
        return false;
    }

    qoi_change_byte_order(&image->header.width, sizeof(image->header.width));
    qoi_change_byte_order(&image->header.height, sizeof(image->header.height));

    return true;
}

bool qoi_load_image_data(FILE *fd, qoi_image* image) {
    if (fseek(fd, 0, SEEK_END) < 0) {
        fprintf(stderr, "[ERROR]: Couldn't jump to end of file!\n");
        return false;
    }
    const size_t data_size = ftell(fd) - QOI_HEADER_SIZE - QOI_END_SIZE;
    uint8_t *data = (uint8_t*)malloc(data_size);
    
    if (data == NULL) {
        fprintf(stderr, "[ERROR]: Couldn't allocate space for data (size: %zu)!\n", data_size);
        return false;
    }
    memset(data, 0, data_size);
    
    fseek(fd, QOI_HEADER_SIZE, SEEK_SET);
    size_t data_read_count = fread(data, 1, data_size, fd);
    if (data_read_count < data_size) {
        fprintf(stderr, "[ERROR]: Read data size (%zu) doesn't match seeked size (%zu)!\n", data_read_count, data_size);
        return false;
    }
    
    uint8_t end[QOI_END_SIZE];
    size_t read_end_size = fread(end, 1, QOI_END_SIZE, fd);
    if (QOI_END_SIZE > read_end_size) {
        fprintf(stderr, "[ERROR]: Read end size (%zu) doesn't match expected end size (%u)!\n", read_end_size, QOI_END_SIZE);
        return false;
    }
    if (0 != memcmp(QOI_END, end, QOI_END_SIZE)) {
        fprintf(stderr, "[ERROR]: Incorrect end magic!\n");
        return false;
    }

    qoi_rgba lookup_array[64] = {0};
    qoi_rgba prev_px = { 0, 0, 0, 255 };

    for (;image->image_data.count < image->header.width * image->header.height; data++) {
        lookup_array[qoi_hash(&prev_px)] = prev_px;

        if (*data == RGBA) {
            prev_px.r = *++data;
            prev_px.g = *++data;
            prev_px.b = *++data;
            prev_px.a = *++data;
        }
        else if (*data == RGB) {
            prev_px.r = *++data;
            prev_px.g = *++data;
            prev_px.b = *++data;
        }
        else if ((*data & 0b11000000) == INDEX) {
            uint8_t index = 0b00111111 & *data;
            prev_px = lookup_array[index];
        }
        else if ((*data & 0b11000000) == DIFF) {
            prev_px.r += ((*data >> 4) & 0b00000011) - 2;
            prev_px.g += ((*data >> 2) & 0b00000011) - 2;
            prev_px.b += (*data & 0b00000011)        - 2;
        }
        else if ((*data & 0b11000000) == LUMA) {
            int8_t dg    = (*data++ & 0b00111111)    - 32;
            int8_t dr_dg = (*data >> 4 & 0b00001111) - 8;
            int8_t db_dg = (*data & 0b00001111)      - 8;

            prev_px.r += dr_dg + dg;
            prev_px.g += dg;
            prev_px.b += db_dg + dg;
        }
        else if ((*data & 0b11000000) == RUN) {
            uint8_t times = (*data & 0b00111111);
            for(uint8_t repeat = 0; repeat <= times; ++repeat) {
                qoi_da_append(&image->image_data, prev_px);
            }
            continue;
        }
        
        qoi_da_append(&image->image_data, prev_px);
    }
    
    return true;
}

bool qoi_load_image(const char* filepath, qoi_image* image) {
    FILE *fd = fopen(filepath, "rb");

    if (NULL == fd) {
        fprintf(stderr, "[ERROR]: Couldn't open file!\n");
        return false;
    }
    if (!qoi_load_image_header(fd, image)) {
        fprintf(stderr, "[ERROR]: Incorrect header data!\n");
        goto error;
    }
    if (!qoi_load_image_data(fd, image)) {
        fprintf(stderr, "[ERROR]: Incorrect image data!\n");
        goto error;
    }
    if (image->header.width * image->header.height != image->image_data.count) {
        fprintf(stderr, "[ERROR]: Image width (%u) and heigth (%u) doesn't match parsed pixel count (%u)!\n", image->header.width, image->header.height, image->image_data.count);
        goto error;
    }
    
    fclose(fd);
    return true;

error:
    fclose(fd);
    return false;
}

void qoi_free_image(qoi_image* image) {
    free(image->image_data.items);
}

bool qoi_write_image(const char* filepath, uint32_t width, uint32_t height, uint8_t chanels, uint8_t colorspace, qoi_rgba* pixels) {
    FILE *fd = fopen(filepath, "wb");

    if (NULL == fd) {
        fprintf(stderr, "[ERROR]: Couldn't open file: %s\n", filepath);
        return false;
    }

    fwrite(QOI_MAGIC, strlen(QOI_MAGIC), 1, fd);
    fwrite(qoi_change_byte_order(&width, sizeof(width)), sizeof(width), 1, fd);
    fwrite(qoi_change_byte_order(&height, sizeof(height)), sizeof(height), 1, fd);
    fwrite(&chanels, sizeof(chanels), 1, fd);
    fwrite(&colorspace, sizeof(colorspace), 1, fd);
    
    qoi_change_byte_order(&width, sizeof(width));
    qoi_change_byte_order(&height, sizeof(height));
    
    qoi_rgba lookup_array[64] = {0};
    qoi_rgba prev_px = { 0, 0, 0, 255 };
    qoi_rgba *pixels_end = &pixels[width * height];
    
    uint8_t type, hash;
    int8_t dr, dg, db, dr_dg, db_dg;
    for (;pixels < pixels_end; ++pixels) {
        hash = qoi_hash(pixels);
        
        if (0 == memcmp(pixels, &prev_px, sizeof(qoi_rgba))) { // RUN
            for (type = RUN - 1;pixels < pixels_end && 0 == memcmp(pixels, &prev_px, sizeof(qoi_rgba)); ++type, ++pixels) {
                if (type == RGB - 1) {
                    fwrite(&type, sizeof(type), 1, fd);
                    type = RUN - 1;
                }
            }
            fwrite(&type, sizeof(type), 1, fd);
            --pixels;
        }
        else if (0 == memcmp(&lookup_array[hash], pixels, sizeof(qoi_rgba))) { // INDEX
            fwrite(&hash, sizeof(hash), 1, fd);
        }
        else if (pixels->a != prev_px.a) { // RGBA
            type = RGBA;
            fwrite(&type, sizeof(type), 1, fd);
            fwrite(pixels, sizeof(qoi_rgba), 1, fd);
        }
        else {
            dr = pixels->r - prev_px.r;
            dg = pixels->g - prev_px.g;
            db = pixels->b - prev_px.b;
            dr_dg = dr - dg;
            db_dg = db - dg;
            
            if (qoi_between(dr, -2, 1) && qoi_between(dg, -2, 1) && qoi_between(db, -2, 1)) { // DIFF
                type = DIFF | (dr + 2) << 4 | (dg + 2) << 2 | (db + 2);
                fwrite(&type, sizeof(type), 1, fd);
            }
            else if (qoi_between(dg, -32, 31) && qoi_between(dr_dg, -8, 7) && qoi_between(db_dg, -8, 7)) { // LUMA
                uint8_t luma[] = { LUMA | (dg + 32), (dr_dg + 8) << 4 | (db_dg + 8) };
                fwrite(luma, sizeof(luma), 1, fd);
            }
            else if (pixels->a == prev_px.a) { // RGB
                type = RGB;
                fwrite(&type, sizeof(type), 1, fd);
                fwrite(pixels, sizeof(qoi_rgba) - sizeof(pixels->a), 1, fd);
            }
        }
        
        prev_px = *pixels;
        lookup_array[hash] = prev_px;
    }

    fwrite(QOI_END, QOI_END_SIZE, 1, fd);
    fclose(fd);
    return true;
}

#endif // QOI_IMPLEMENTATION