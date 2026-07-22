#ifndef DRIVERS_VIDEO_FONT_PSF_H
#define DRIVERS_VIDEO_FONT_PSF_H

#include <kernel/types.h>

#define PSF1_MAGIC 0x0436
#define PSF2_MAGIC 0x864ab572

typedef struct {
    uint16_t magic;
    uint8_t mode;
    uint8_t charsize;
} psf1_header_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t headersize;
    uint32_t flags;
    uint32_t numglyph;
    uint32_t bytesperglyph;
    uint32_t height;
    uint32_t width;
} psf2_header_t;

int font_psf_init(const void *font_data, size_t font_size);
const uint8_t *font_psf_get_glyph(uint32_t c);
int font_psf_get_width(void);
int font_psf_get_height(void);

#endif /* DRIVERS_VIDEO_FONT_PSF_H */
