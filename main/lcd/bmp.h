#ifndef BMP_H
#define BMP_H

#define BMP_MAGIC 19778 // 0x4D42 'BM' 19778

#define BMP_GET_PADDING(a) ((a) % 4)

#include <stdio.h>

enum bmp_error {
    BMP_FILE_NOT_OPENED = -4,
    BMP_HEADER_NOT_INITIALIZED,
    BMP_INVALID_FILE,
    BMP_ERROR,
    BMP_OK = 0
};

typedef struct {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t rgbReserved;
} RGBQUAD;

typedef struct {
    // BITMAPFILEHEADER
    uint16_t bfType; // 0x4D42 'BM' 19778
    uint32_t bfSize; // 文件的大小，用字节为单位
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits; // 从文件头开始到实际的图像数据之间的字节的偏移量

    // BITMAPINFOHEADER 位图信息头
    uint32_t biSize; // BITMAPINFOHEADER 大小
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
    // 到这儿size = 54

    // RGBQUAD 彩色表 // 1、4、8、16 才有
    // pColor = ((LPSTR) pBitmapInfo + (uint16_t) (pBitmapInfo->bmiHeader.biSize))
    RGBQUAD *bmiColors;
} bmp_header;

// biBitCount = 1 调色板大小2
// biBitCount = 4 调色板大小16
// biBitCount = 8 调色板大小256
// biBitCount = 16 & biCompression = BI_RGB 它没有调色板 颜色格式是 bmp_pixel_16
//                   biCompression = BI_BITFIELDS  色板的位置被三个DWORD变量占据，称为红、绿、蓝掩码
// biBitCount = 24, 32 后面直接是数据

// 1 4 8 查表

// bgr 个5 位 最高位0
typedef struct {
    uint16_t bgr555;
} bmp_pixel_16;

typedef struct {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
} bmp_pixel_24;

typedef struct {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t reserve;
} bmp_pixel_32;

// This is faster than a function call
#define BMP_PIXEL_24(r, g, b) ((bmp_pixel_24){(b),(g),(r)})

typedef struct {
    bmp_header img_header;
    bmp_pixel_24 **img_pixels;
} bmp_img_24;

// BMP_HEADER
void bmp_header_init_df(bmp_header *, const int, const int);

enum bmp_error bmp_header_write(const bmp_header *, FILE *);

enum bmp_error bmp_header_read(bmp_header *, FILE *);

// BMP_PIXEL
void bmp_pixel_init(bmp_pixel_24 *, const unsigned char, const unsigned char, const unsigned char);

// BMP_IMG
void bmp_img_alloc(bmp_img_24 *);

void bmp_img_init_df(bmp_img_24 *, const int, const int);

void bmp_img_free(bmp_img_24 *);

enum bmp_error bmp_img_write(const bmp_img_24 *, const char *);

enum bmp_error bmp_img_read(bmp_img_24 *, const char *);

#endif