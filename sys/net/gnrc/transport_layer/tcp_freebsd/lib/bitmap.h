#ifndef BITMAP_H_
#define BITMAP_H_

#include <stdint.h>
#include <stdlib.h>

#define BITS_TO_BYTES(bits) (((bits) >> 3) + (((bits) & 0x7) ? 1 : 0))

void bmp_init(uint8_t* buf, size_t numbytes);
void bmp_setrange(uint8_t* buf, size_t start, size_t len);
void bmp_clrrange(uint8_t* buf, size_t start, size_t len);
size_t bmp_countset(uint8_t* buf, size_t buflen, size_t start, size_t limit);
int bmp_isempty(uint8_t* buf, size_t buflen);
void bmp_print(uint8_t* buf, size_t buflen);

#endif
