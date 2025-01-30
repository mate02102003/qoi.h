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

#define mod(a, b) (a%b + b) % b

#define qoi_da_append(da, item)                                                          \
    do {                                                                                 \
        if ((da)->count >= (da)->capacity) {                                             \
            (da)->capacity = (da)->capacity == 0 ? QOI_DA_INIT_CAP : (da)->capacity*2;   \
            (da)->items = realloc((da)->items, (da)->capacity*sizeof(*(da)->items));     \
        }                                                                                \
                                                                                         \
        (da)->items[(da)->count++] = (item);                                             \
    } while (0)

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
    Pixel*   items;
} Pixels;

typedef struct {
    qoi_header  header;
    Pixels      image_data;
} qoi_image;

inline uint8_t qoi_hash(qoi_rgba *color);
int qoi_load_image_header(int fd, qoi_image *image);
int qoi_load_image_data(int fd, qoi_image *image);
int qoi_load_image(const char *filepath, qoi_image *image);
void qoi_unload_image(qoi_image *image);
int qoi_write_image_from_image(const char *filepath, qoi_image image);
int qoi_write_image_from_header(const char *filepath, qoi_header header, Pixel **pixels);
int qoi_write_image(const char *filepath, uint32_t width, uint32_t height, uint8_t chanels, uint8_t colorspace, Pixel **pixels);

#endif // QOI_HEADER
#ifdef QOI_IMPLEMENTATION

inline uint8_t qoi_hash(qoi_rgba *color) {
    return (color->r * 3 + color->g * 5 + color->b * 7 + color->a * 11) % 64;
}

int qoi_load_image_header(int fd, qoi_image* image) {
    if (read(fd, &image->header, QOI_HEADER_SIZE) != QOI_HEADER_SIZE) {
        return -1;
    }

    if (memcmp(image->header.magic, QOI_MAGIC, 4) != 0) {
        return -1;
    }

    // change from big endian to little endian
    // code from https://stackoverflow.com/questions/2182002/how-to-convert-big-endian-to-little-endian-in-c-without-using-library-functions
    image->header.width = ((image->header.width>>24)&0xff) | // move byte 3 to byte 0
                          ((image->header.width<<8)&0xff0000) | // move byte 1 to byte 2
                          ((image->header.width>>8)&0xff00) | // move byte 2 to byte 1
                          ((image->header.width<<24)&0xff000000); // byte 0 to byte 3
    image->header.height = ((image->header.height>>24)&0xff) | // move byte 3 to byte 0
                           ((image->header.height<<8)&0xff0000) | // move byte 1 to byte 2
                           ((image->header.height>>8)&0xff00) | // move byte 2 to byte 1
                           ((image->header.height<<24)&0xff000000); // byte 0 to byte 3

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

    Pixel lookup_array[64];
    memset(lookup_array, 0, sizeof(lookup_array));
    Pixel cur_px  = { 0, 0, 0, 255 };
    Pixel prev_px = cur_px;

    do {
        if ((*data & RGBA) == RGBA) {
            cur_px.r = *++data;
            cur_px.g = *++data;
            cur_px.b = *++data;
            cur_px.a = *data;
        }
        else if ((*data & RGB) == RGB) {
            cur_px.r = *++data;
            cur_px.g = *++data;
            cur_px.b = *data;
        }
        else if ((*data & 0b11000000) == INDEX) {
            uint8_t index = 0b00111111 & *data;
            cur_px = lookup_array[index];
        }
        else if ((*data & 0b11000000) == DIFF) {
            cur_px.r = mod(prev_px.r + ((*data << 4) & 0b00000011) - 2, 255);
            cur_px.g = mod(prev_px.g + ((*data << 2) & 0b00000011) - 2, 255);
            cur_px.b = mod(prev_px.b + (*data & 0b00000011) - 2, 255);
        }
        else if ((*data & 0b11000000) == LUMA) {
            int8_t dg = (*data++ & 0b00111111) - 32;
            int8_t dr = (*data << 4 & 0b00001111) - 8;
            int8_t db = (*data & 0b00001111) - 8;

            cur_px.r = mod(prev_px.r + dr, 255);
            cur_px.g = mod(prev_px.g + dg, 255);
            cur_px.b = mod(prev_px.b + db, 255);
        }
        else if ((*data & 0b11000000) == RUN) {
            uint8_t times = (*data & 0b00111111);
            for(uint8_t repeat = 0; repeat < times; ++repeat) {
                qoi_da_append(&image->image_data, cur_px);
            }
        }
        else {
            return -1;
        }
        
        lookup_array[qoi_hash(&cur_px)] = cur_px;
        qoi_da_append(&image->image_data, cur_px);
        prev_px = cur_px;
    } while (*++data);

    return 0;
}

int qoi_load_image(const char* filepath, qoi_image* image) {
    int fd = open(filepath, O_RDONLY);

    if (-1 == fd) {
        fprintf(stderr, "[ERROR]: %s\n", "Couldn't open file!");
        goto error;
    }
    if (-1 == qoi_load_image_header(fd, image)) {
        fprintf(stderr, "[ERROR]: %s\n", "Incorrect header data!");
        goto error;
    }
    if (-1 == qoi_load_image_data(fd, image)) {
        fprintf(stderr, "[ERROR]: %s\n", "Incorrect image data!");
        goto error;
    }
    
    close(fd);
    return 0;

error:
    close(fd);
    return -1;
}

void qoi_unload_image(qoi_image* image) {
    free(image->image_data.items);
}

int qoi_write_image_from_image(const char* filepath, qoi_image image){
    return qoi_write_image_from_header(filepath, image.header, &image.image_data.items);
}

int qoi_write_image_from_header(const char* filepath, qoi_header header, Pixel** pixels) {
    return qoi_write_image(filepath, header.width, header.height, header.chanels, header.colorspace, pixels);
}

int qoi_write_image(const char* filepath, uint32_t width, uint32_t height, uint8_t chanels, uint8_t colorspace, Pixel** pixels) {
    int fd = open(filepath, O_WRONLY);

    if (-1 == fd) {
        perror("Couldn't open file!");
        goto error;
    }

    write(fd, QOI_MAGIC, strlen(QOI_MAGIC));
    write(fd, &width, sizeof(width));
    write(fd, &height, sizeof(height));
    write(fd, &chanels, sizeof(chanels));
    write(fd, &colorspace, sizeof(colorspace));

    Pixel lookup_array[64];
    memset(lookup_array, 0, sizeof(lookup_array));
    Pixel prev_px = { 0, 0, 0, 255 };
    uint8_t same_pixel = RUN;

    do {
        if (same_pixel == RGB) {
            same_pixel--;
            write(fd, &same_pixel, 1);
            same_pixel = RUN + 1;
        }
        if (memcmp(*pixels, *pixels, sizeof(Pixel))) {
            same_pixel++;
            continue;
        }
        else {
            write(fd, &same_pixel, 1);
            same_pixel = RUN; 
        }

        uint8_t hash = qoi_hash(*pixels);
        if (memcmp(&lookup_array[hash], *pixels, sizeof(Pixel))) {
            write(fd, &hash, 1);
        }

        prev_px = **pixels;
        lookup_array[hash] = prev_px;
    } while (*++pixels);

    close(fd);
    return 0;
error:
    close(fd);
    return -1;
}

#endif // QOI_IMPLEMENTATION