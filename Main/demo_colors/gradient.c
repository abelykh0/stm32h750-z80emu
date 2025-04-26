#include "gradient.h"
#include "config.h"

void gradient(uint8_t* indexed_image, uint32_t* clut)
{
	for (uint32_t color = 0; color < 4; color++)
	{
		for (uint32_t brightness = 0; brightness < 64; brightness++)
		{
			uint32_t r = 0;
			uint32_t g = 0;
			uint32_t b = 0;

			switch (color)
			{
			case 1:
				r = (brightness << 2) | 0x03;
				break;
			case 2:
				g = (brightness << 2) | 0x03;
				break;
			case 3:
				b = (brightness << 2) | 0x03;
				break;
			default:
				// white
				r = (brightness << 2) | 0x03;
				g = (brightness << 2) | 0x03;
				b = (brightness << 2) | 0x03;
				break;
			}

			clut[(color << 6) | brightness] = (0xFF << 24) | (r << 16) | (g << 8) | b;
		}
	}

	for (uint32_t y = 0; y < V_SIZE; y++)
	{
		uint32_t color = y / 100;
		for (uint32_t brightness = 0; brightness < 64; brightness++)
		{
			for (uint32_t x = brightness * 11; x < (brightness + 1) * 11; x++)
			{
				indexed_image[(y * H_SIZE) + x] = (color << 6) | brightness;
			}
		}
	}
}
