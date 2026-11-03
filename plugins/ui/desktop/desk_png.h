#ifndef DESK_PNG_H
#define DESK_PNG_H

#include <stdio.h>

/* Writes w x h packed RGB (3 bytes a pixel, rows top down) to f as one PNG; 1 on success. */
int desk_png_write(FILE* f, int w, int h, const unsigned char* rgb);

#endif
