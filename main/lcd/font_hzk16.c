#include <stdio.h>

#include "fonts.h"

extern const uint8_t hzk_16_font_start[] asm("_binary_HZK16_bin_start");
extern const uint8_t hzk_16_font_end[] asm("_binary_HZK16_bin_end");

sFONT Font_HZK16 = {
        hzk_16_font_start,
        16, /* Width */
        16, /* Height */
        0,
        .is_chinese = 1
};