#include "../include/xivres/util.dxt.h"

void xivres::util::DecompressBlockDXT1(uint32_t x, uint32_t y, uint32_t width, const uint8_t* blockStorage, b8g8r8a8* image) {
	uint16_t color0 = *reinterpret_cast<const uint16_t*>(blockStorage);
	uint16_t color1 = *reinterpret_cast<const uint16_t*>(blockStorage + 2);

	uint32_t temp;

	temp = (color0 >> 11) * 255 + 16;
	uint8_t r0 = (uint8_t)((temp / 32 + temp) / 32);
	temp = ((color0 & 0x07E0) >> 5) * 255 + 32;
	uint8_t g0 = (uint8_t)((temp / 64 + temp) / 64);
	temp = (color0 & 0x001F) * 255 + 16;
	uint8_t b0 = (uint8_t)((temp / 32 + temp) / 32);

	temp = (color1 >> 11) * 255 + 16;
	uint8_t r1 = (uint8_t)((temp / 32 + temp) / 32);
	temp = ((color1 & 0x07E0) >> 5) * 255 + 32;
	uint8_t g1 = (uint8_t)((temp / 64 + temp) / 64);
	temp = (color1 & 0x001F) * 255 + 16;
	uint8_t b1 = (uint8_t)((temp / 32 + temp) / 32);

	uint32_t code = *reinterpret_cast<const uint32_t*>(blockStorage + 4);

	for (int j = 0; j < 4; j++) {
		for (int i = 0; i < 4; i++) {
			b8g8r8a8 finalColor;
			uint8_t positionCode = (code >> 2 * (4 * j + i)) & 0x03;

			if (color0 > color1) {
				switch (positionCode) {
					case 0:
						finalColor = b8g8r8a8(r0, g0, b0, 255);
						break;
					case 1:
						finalColor = b8g8r8a8(r1, g1, b1, 255);
						break;
					case 2:
						finalColor = b8g8r8a8((2 * r0 + r1) / 3, (2 * g0 + g1) / 3, (2 * b0 + b1) / 3, 255);
						break;
					case 3:
						finalColor = b8g8r8a8((r0 + 2 * r1) / 3, (g0 + 2 * g1) / 3, (b0 + 2 * b1) / 3, 255);
						break;
				}
			} else {
				switch (positionCode) {
					case 0:
						finalColor = b8g8r8a8(r0, g0, b0, 255);
						break;
					case 1:
						finalColor = b8g8r8a8(r1, g1, b1, 255);
						break;
					case 2:
						finalColor = b8g8r8a8((r0 + r1) / 2, (g0 + g1) / 2, (b0 + b1) / 2, 255);
						break;
					case 3:
						finalColor = b8g8r8a8(0, 0, 0, 255);
						break;
				}
			}

			if (x + i < width)
				image[(y + j) * width + (x + i)] = finalColor;
		}
	}
}

void xivres::util::BlockDecompressImageDXT1(uint32_t width, uint32_t height, const uint8_t* blockStorage, b8g8r8a8* image) {
	uint32_t blockCountX = (width + 3) / 4;
	uint32_t blockCountY = (height + 3) / 4;
	uint32_t blockWidth = (width < 4) ? width : 4;
	uint32_t blockHeight = (height < 4) ? height : 4;

	for (uint32_t j = 0; j < blockCountY; j++) {
		for (uint32_t i = 0; i < blockCountX; i++) DecompressBlockDXT1(i * 4, j * 4, width, blockStorage + i * 8, image);
		blockStorage += blockCountX * 8;
	}
}

void xivres::util::DecompressBlockDXT5(uint32_t x, uint32_t y, uint32_t width, const uint8_t* blockStorage, b8g8r8a8* image) {
	uint8_t alpha0 = *reinterpret_cast<const uint8_t*>(blockStorage);
	uint8_t alpha1 = *reinterpret_cast<const uint8_t*>(blockStorage + 1);

	const uint8_t* bits = blockStorage + 2;
	uint32_t alphaCode1 = bits[2] | (bits[3] << 8) | (bits[4] << 16) | (bits[5] << 24);
	uint16_t alphaCode2 = bits[0] | (bits[1] << 8);

	uint16_t color0 = *reinterpret_cast<const uint16_t*>(blockStorage + 8);
	uint16_t color1 = *reinterpret_cast<const uint16_t*>(blockStorage + 10);

	uint32_t temp;

	temp = (color0 >> 11) * 255 + 16;
	uint8_t r0 = (uint8_t)((temp / 32 + temp) / 32);
	temp = ((color0 & 0x07E0) >> 5) * 255 + 32;
	uint8_t g0 = (uint8_t)((temp / 64 + temp) / 64);
	temp = (color0 & 0x001F) * 255 + 16;
	uint8_t b0 = (uint8_t)((temp / 32 + temp) / 32);

	temp = (color1 >> 11) * 255 + 16;
	uint8_t r1 = (uint8_t)((temp / 32 + temp) / 32);
	temp = ((color1 & 0x07E0) >> 5) * 255 + 32;
	uint8_t g1 = (uint8_t)((temp / 64 + temp) / 64);
	temp = (color1 & 0x001F) * 255 + 16;
	uint8_t b1 = (uint8_t)((temp / 32 + temp) / 32);

	uint32_t code = *reinterpret_cast<const uint32_t*>(blockStorage + 12);

	for (int j = 0; j < 4; j++) {
		for (int i = 0; i < 4; i++) {
			int alphaCodeIndex = 3 * (4 * j + i);
			int alphaCode;

			if (alphaCodeIndex <= 12) {
				alphaCode = (alphaCode2 >> alphaCodeIndex) & 0x07;
			} else if (alphaCodeIndex == 15) {
				alphaCode = (alphaCode2 >> 15) | ((alphaCode1 << 1) & 0x06);
			} else // alphaCodeIndex >= 18 && alphaCodeIndex <= 45
			{
				alphaCode = (alphaCode1 >> (alphaCodeIndex - 16)) & 0x07;
			}

			uint8_t finalAlpha;
			if (alphaCode == 0) {
				finalAlpha = alpha0;
			} else if (alphaCode == 1) {
				finalAlpha = alpha1;
			} else {
				if (alpha0 > alpha1) {
					finalAlpha = ((8 - alphaCode) * alpha0 + (alphaCode - 1) * alpha1) / 7;
				} else {
					if (alphaCode == 6)
						finalAlpha = 0;
					else if (alphaCode == 7)
						finalAlpha = 255;
					else
						finalAlpha = ((6 - alphaCode) * alpha0 + (alphaCode - 1) * alpha1) / 5;
				}
			}

			uint8_t colorCode = (code >> 2 * (4 * j + i)) & 0x03;

			b8g8r8a8 finalColor;
			switch (colorCode) {
				case 0:
					finalColor = b8g8r8a8(r0, g0, b0, finalAlpha);
					break;
				case 1:
					finalColor = b8g8r8a8(r1, g1, b1, finalAlpha);
					break;
				case 2:
					finalColor = b8g8r8a8((2 * r0 + r1) / 3, (2 * g0 + g1) / 3, (2 * b0 + b1) / 3, finalAlpha);
					break;
				case 3:
					finalColor = b8g8r8a8((r0 + 2 * r1) / 3, (g0 + 2 * g1) / 3, (b0 + 2 * b1) / 3, finalAlpha);
					break;
			}

			if (x + i < width)
				image[(y + j) * width + (x + i)] = finalColor;
		}
	}
}

void xivres::util::BlockDecompressImageDXT5(uint32_t width, uint32_t height, const uint8_t* blockStorage, b8g8r8a8* image) {
	uint32_t blockCountX = (width + 3) / 4;
	uint32_t blockCountY = (height + 3) / 4;
	uint32_t blockWidth = (width < 4) ? width : 4;
	uint32_t blockHeight = (height < 4) ? height : 4;

	for (uint32_t j = 0; j < blockCountY; j++) {
		for (uint32_t i = 0; i < blockCountX; i++) DecompressBlockDXT5(i * 4, j * 4, width, blockStorage + i * 16, image);
		blockStorage += blockCountX * 16;
	}
}
