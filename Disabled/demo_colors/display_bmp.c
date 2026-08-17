#include <stdio.h>
#include <stdint.h>
#include "display_bmp.h"

#include <stdint.h>

#pragma pack(push, 1)  // exact layout without padding

typedef struct {
    uint16_t bfType;      // Signature "BM" (0x4D42)
    uint32_t bfSize;      // Size of the file (in bytes)
    uint16_t bfReserved1; // Reserved, must be 0
    uint16_t bfReserved2; // Reserved, must be 0
    uint32_t bfOffBits;   // Offset to start of pixel data
} BMPFileHeader;

typedef struct {
    uint32_t biSize;          // Size of this header (40 bytes)
    int32_t  biWidth;         // Bitmap width in pixels
    int32_t  biHeight;        // Bitmap height in pixels
    uint16_t biPlanes;        // Number of planes (must be 1)
    uint16_t biBitCount;      // Bits per pixel (1, 4, 8, 16, 24, or 32)
    uint32_t biCompression;   // Compression method (0 = BI_RGB, no compression)
    uint32_t biSizeImage;     // Size of pixel data (can be 0 if uncompressed)
    int32_t  biXPelsPerMeter; // Horizontal resolution (pixels per meter)
    int32_t  biYPelsPerMeter; // Vertical resolution (pixels per meter)
    uint32_t biClrUsed;       // Number of colors in the color table (0 = all)
    uint32_t biClrImportant;  // Number of important colors (0 = all)
} BMPInfoHeader;

#pragma pack(pop)

// Function to load BMP pixel data into an indexed image
int load_bmp_image(FIL* bmp_file, uint8_t* indexed_image, uint32_t* clut) {
	// File header
	f_lseek(bmp_file, sizeof(BMPFileHeader));

	// Infor header
	BMPInfoHeader infoHeader;
    UINT bytesRead = sizeof(BMPInfoHeader);
    f_read(bmp_file, &infoHeader, bytesRead, &bytesRead);

    // Read color table (256 entries for 8-bit BMP)
    for (int i = 0; i < 256; i++) {
        uint8_t color[4];
        //fread(color, 4, 1, bmp_file);
        UINT bytesRead = 4;
        f_read(bmp_file, color, bytesRead, &bytesRead);
        clut[i] = (color[2] << 16) | (color[1] << 8) | color[0]; // Store as RGB
    }

    int32_t height = infoHeader.biHeight;
    int32_t width = infoHeader.biWidth;

    // Read pixel data (indexed)
    uint8_t row[width];
    int row_start;
    int row_end;
    int step;

    // Check if the height is negative (image is top-to-bottom)
    if (height < 0) {
        // Image is top-to-bottom, read from top to bottom
        row_start = 0;
        row_end = -height; // Convert to positive for processing
        step = 1;
    } else {
        // Image is bottom-to-top (standard BMP), read from bottom to top
        row_start = height - 1;
        row_end = 0;
        step = -1;
    }

    // Process rows based on row order
    for (int y = row_start; (infoHeader.biHeight < 0 ? y < row_end : y >= row_end); y += step) {
        //fread(row, width, 1, bmp_file);
        UINT bytesRead = width;
        f_read(bmp_file, row, bytesRead, &bytesRead);
        for (int x = 0; x < width; x++) {
            indexed_image[y * width + x] = row[x];
        }
    }

    return 0;
}
