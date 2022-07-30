# My Super Bike

`idf.py build`

`idf.py -p COM4 build flash monitor`

---

MIC
- IO7 -- WS 
- IO15 -- DATA 
- IO16 -- SCK

WS2812
- IO38

SD
- IO3 SCK
- IO4 MOSI
- IO0 MISO
- IO8 CS

PSRAM
- IO35
- IO36
- IO37

SPL06 I2C
- IO39 I2C_SCL
- IO40 I2C_SDA

GPS
- IO17 UART_TXD
- IO18 UART_RXD

SPI LCD
- IO1 RST
- IO2 BUSY
- IO9 DC
- IO10 CS
- IO11 MOSI
- IO12 CLK

ZW800
- IO45 - TOUCH
- IO48 - RX
- IO47 - TX

FREE
- IO5
- IO6
- IO13
- IO14
- IO19
- IO20
- IO21
- IO41
- IO42

---
- [lvgl](https://docs.lvgl.io/master/intro/index.html)
- [esp-idf](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/get-started/index.html)
