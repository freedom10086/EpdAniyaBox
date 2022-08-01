set(srcs "lcd/font8.c" "lcd/font12.c" "lcd/font16.c" "lcd/font16_2.c" "lcd/font20.c" "lcd/font24.c" "lcd/font20_2.c" "lcd/font32.c" "lcd/font32_2.c" "lcd/font36.c" "lcd/epdpaint.c" "lcd/font_hzk16.c"
        "tools/kalman_filter.c" "tools/encode.c"
        "sd_card.c" "gpx_writer.c"  "ble/ble_csc.c" "ble/ble_hrm.c" "ble/ble_device.c"
        "led/led_strip_encoder.c" "led/ws2812.c"
        "pressure_common.c" "ms5611.c" "spl06.c"
        "zw800.c"
        "nmea_parser.c"
        "lcd/epd_lcd_ssd1680.c" "lcd/main_page.c" "lcd/test_page.c" "lcd/display.c"
        "main.c")

idf_component_register(SRCS ${srcs}
        EMBED_FILES "lcd/HZK16.bin"
        INCLUDE_DIRS ".")