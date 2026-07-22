/* tach Operating System - PSF Font Header */
#ifndef _DRIVERS_FONT_PSF_H
#define _DRIVERS_FONT_PSF_H

#include <kernel/types.h>
#include <stdint.h>

/* PSF font header structure */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t headersize;
    uint32_t flags;
    uint32_t numglyph;
    uint32_t bytesperglyph;
    uint32_t height;
    uint32_t width;
} psf_font_t;

/* Load PSF font */
int psf_load(const void* data, size_t size, psf_font_t* font);

/* Get glyph bitmap */
const uint8_t* psf_get_glyph(psf_font_t* font, uint32_t index);

#endif /* _DRIVERS_FONT_PSF_H */
