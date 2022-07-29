idf_component_register(SRCS
        "display/font8.c" "display/font12.c" "display/font16.c" "display/font16_2.c" "display/font20.c" "display/font24.c" "display/font20_2.c" "display/font32.c" "display/font32_2.c" "display/font36.c" "display/epdpaint.c"
        "tools/kalman_filter.c"
        "sd_card.c" "gpx_writer.c"  "ble/ble_csc.c" "ble/ble_hrm.c" "ble/ble_device.c"
        "led/led_strip_encoder.c" "led/ws2812.c"
        "pressure_common.c" "ms5611.c" "spl06.c"
        "zw800.c"
        "nmea_parser.c"
        "display/epd_lcd_ssd1680.c" "display/main_page.c" "display/display.c" "main.c"
                    INCLUDE_DIRS ".")