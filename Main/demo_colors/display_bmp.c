#include <stdio.h>
#include <stdint.h>
#include "display_bmp.h"

// Function to load BMP pixel data into an indexed image
int load_bmp_image(FIL* bmp_file, uint8_t* indexed_image, uint32_t* clut, int width, int height) {
	// Skip header
	f_lseek(bmp_file, 54);

    // Read color table (256 entries for 8-bit BMP)
    for (int i = 0; i < 256; i++) {
        uint8_t color[4];
        //fread(color, 4, 1, bmp_file);
        UINT bytesRead = 4;
        f_read(bmp_file, color, bytesRead, &bytesRead);
        clut[i] = (color[2] << 16) | (color[1] << 8) | color[0]; // Store as RGB
    }

    // Read pixel data (indexed)
    uint8_t row[width];
    int row_start = 0;
    int row_end = height - 1;
    int step = 1;

    // Check if the height is negative (image is top-to-bottom)
    if (height < 0) {
        // Image is top-to-bottom, read from top to bottom
        row_start = 0;
        row_end = height * -1; // Convert to positive for processing
        step = 1;
    } else {
        // Image is bottom-to-top (standard BMP), read from bottom to top
        row_start = height - 1;
        row_end = -1;
        step = -1;
    }

    // Process rows based on row order
    for (int y = row_start; (height < 0 ? y < row_end : y >= row_end); y += step) {
        //fread(row, width, 1, bmp_file);
        UINT bytesRead = width;
        f_read(bmp_file, row, bytesRead, &bytesRead);
        for (int x = 0; x < width; x++) {
            indexed_image[y * width + x] = row[x];
        }
    }

    return 0;
}
