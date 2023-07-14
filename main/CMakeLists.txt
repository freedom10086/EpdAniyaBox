set(srcs "lcd/font8.c" "lcd/font12.c" "lcd/font16.c" "lcd/font20.c" "lcd/font24.c" "lcd/font_hzk16.c" "lcd/epdpaint.c"
        "bike_common.c"
        "lcd/bmp.c" "lcd/jpg.c"
        "tools/kalman_filter.c" "tools/encode.c"
        "battery.c" "key.c"
        "wifi/wifi_ap.c" "wifi/my_http_server.c" "wifi/my_http_file_server.c" "wifi/my_mount.c" "wifi/my_wifi_ota.c"
        "ble/ble_csc.c" "ble/ble_hrm.c" "ble/ble_device.c" "ble/misc.c" "ble/peer.c"
        "sht31.c"
        "view/list_view.c" "view/digi_view.c" "view/battery_view.c"
        "lcd/epd_lcd_ssd1680.c" "page/menu_page.c" "page/test_page.c" "page/info_page.c" "lcd/display.c" "page/image_page.c" "page/temperature_page.c" "page/upgrade_page.c"
        "page/manual_page.c" "page/image_manage_page.c" "page/ble_device_page.c" "page/setting_list_page.c"
        "page_manager.c"
        "main.c")

idf_component_register(SRCS ${srcs}
        EMBED_FILES "lcd/HZK16.bin"
        EMBED_FILES "static/favicon.ico" "static/ota.html" "static/upload_script.html"
        EMBED_FILES "static/aniya_200.bmp" "static/aniya_200_1.bmp"
        EMBED_FILES "static/icon_wifi.bmp" "static/icon_ble.bmp" "static/icon_sat.bmp" "static/icon_upgrade.bmp"
        EMBED_FILES "static/ic_close_32.bmp" "static/ic_home_32.bmp" "static/ic_image_32.bmp" "static/ic_info_32.bmp" "static/ic_manual_32.bmp"
        EMBED_FILES "static/ic_setting_32.bmp" "static/ic_upgrade_32.bmp" "static/ic_reboot_32.bmp" "static/ic_back_32.bmp" "static/ic_ble_32.bmp"
        INCLUDE_DIRS ".")