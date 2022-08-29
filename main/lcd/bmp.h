#ifndef BMP_H
#define BMP_H

#define BMP_MAGIC 19778 // 0x4D42 'BM' 19778

#define BMP_GET_PADDING(a) ((a) % 4)

#include <stdio.h>
#include "epdpaint.h"

enum bmp_error {
    BMP_NOT_SUPPORTED_FORMAT = -6,
    BMP_FILE_NOT_OPENED,
    BMP_HEADER_NOT_INITIALIZED,
    BMP_INVALID_FILE,
    BMP_INVALID_LUT_SIZE,
    BMP_ERROR,
    BMP_OK = 0
};

// This is faster than a function call
#define BMP_PIXEL_24(r, g, b) ((bmp_pixel_24){(b),(g),(r)})

// https://freeimage.sourceforge.io/fnet/html/C116A9BC.htm
enum bmp_compression_type {
    BI_RGB = 0, // no compress
    BI_RLE8,
    BI_RLE4,
    BI_BITFIELDS, // (16 & 32)
};

typedef struct {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t rgbReserved;
} __attribute__((packed)) RGBQUAD;

typedef struct {
    uint32_t blue_mask;
    uint32_t green_mask;
    uint32_t red_mask;
} __attribute__((packed)) RGBQUAD_COLOR_MASK;

typedef struct {
    // BITMAPFILEHEADER  大小 14
    uint16_t bfType; // 0x4D42 'BM' 19778
    uint32_t bfSize; // 文件的大小，用字节为单位 DWORD
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits; // 从文件头开始到实际的图像数据之间的字节的偏移量 62

    // BITMAPINFOHEADER 位图信息头
    uint32_t biSize; // BITMAPINFOHEADER 大小 40
    int32_t biWidth; // 图像的宽度 以像素为单位
    int32_t biHeight; // 图像的高度, 如果负数则说明图像是正向的
    uint16_t biPlanes; // 为目标设备说明位面数 always 1
    uint16_t biBitCount; //  比特数/像素, 其值为1、4、8、16、24、或32
    uint32_t biCompression; // 压缩 BI_RGB：没有压缩； BI_RLE8，BI_RLE4：每个像素8/4比特的RLE压缩编码， BI_BITFIELDS：每个像素的比特由指定的掩码决定。
    uint32_t biSizeImage; // 说明图像的大小，以字节为单位。当用BI_RGB格式时，可设置为0
    int32_t biXPelsPerMeter; // 水平分辨率，用像素/米表示
    int32_t biYPelsPerMeter; // 垂直分辨率，用像素/米表示
    uint32_t biClrUsed; // 位图实际使用的彩色表中的颜色索引数
    uint32_t biClrImportant; // 对图像显示有重要影响的颜色索引的数目，如果是0，表示都重要。
    // 到这儿size = 54 , 偏移 62
} __attribute__((packed)) bmp_header;

// biBitCount = 1 调色板大小2
// biBitCount = 4 调色板大小16
// biBitCount = 8 调色板大小256
// biBitCount = 16 & biCompression = BI_RGB 它没有调色板 颜色格式是 bmp_pixel_16
//                   biCompression = BI_BITFIELDS  色板的位置被三个DWORD变量占据，称为红、绿、蓝掩码
// biBitCount = 24, 32 后面直接是数据
typedef struct {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
} __attribute__((packed)) bmp_pixel_24;

typedef struct {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t reserve;
} __attribute__((packed)) bmp_pixel_32;

typedef struct {
    bmp_header img_header;
    uint8_t data[1]; //包含lut和数据具体使用的时候再分开
} __attribute__((packed)) bmp_img_common;

typedef struct {
    bmp_header img_header;
    uint8_t *color_data; //包含lut和数据具体使用的时候再分开
    uint8_t *data_buff;
    uint16_t data_buff_size;  // 必须是按行读取的整数倍
    uint16_t data_start_y; // 数据buff start_y
    uint16_t data_end_y; // 数据buff end_y (不包含)
} __attribute__((packed)) bmp_img_file_common;

//    bfOffBits - 14 - biSize
//    RGBQUAD 彩色表lut // 1、4、8、16 才有
//    pColor = ((LPSTR) pBitmapInfo + (uint16_t) (pBitmapInfo->bmiHeader.biSize))
//    RGBQUAD *bmiColors;
typedef struct {
    bmp_header img_header;
    RGBQUAD bmiColors[2];
    uint8_t pixel[1];
} __attribute__((packed)) bmp_img_1;

typedef struct {
    bmp_header img_header;
    bmp_pixel_24 **img_pixels;
} __attribute__((packed)) bmp_img_24;

typedef struct {
    bmp_header img_header;
    bmp_pixel_32 img_pixels[1];
} __attribute__((packed)) bmp_img_32;

enum bmp_error bmp_header_read(bmp_header *header, uint8_t *data, uint16_t data_len);

void bmp_get_pixel(pixel_color *out_color, bmp_img_common *bmp_img, uint16_t x, uint16_t y);

enum bmp_error bmp_img_read(bmp_img_24 *, const char *);

// FILE

enum bmp_error bmp_header_read_file(bmp_img_file_common *bmp_img, FILE *img_file);

enum bmp_error bmp_file_get_pixel(pixel_color *out_color, bmp_img_file_common *bmp_img, uint16_t x, uint16_t y, FILE *img_file);

void bmp_file_free(bmp_img_file_common *bmp_img);

#endif