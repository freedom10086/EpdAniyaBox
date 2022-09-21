# Epd Aniya Box

`idf.py build`

`idf.py -p COM4 build flash monitor`

---
主控esp32c3(同样支持esp32 esp32s3)

立创开源地址: 
[https://oshwhub.com/freedopm/aniya_epd](https://oshwhub.com/freedopm/aniya_epd)


200 * 200 1in54 墨水屏裸屏尺寸（ssd1680）
- 1.2mm厚
- 显示区域 28mm
- 上左右黑边 1.9mm
- 下黑边 7.6mm
- 下排线预留 2mm
- 37.3 (+ 2mm) * 31.8

SPI LCD
- IO7 RST
- IO8 BUSY
- IO6 DC
- IO5 CS
- IO10 MOSI
- IO4 CLK

按键
- key 1 IO0
- key 2 IO9

温湿度传感器sht31 i2c
- SCL IO2
- SDA IO3

---
- [esp-idf](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/get-started/index.html)

### 分区表

idf.py partition-table print


idf.py erase-flash

idf.py partition-table-flash: will flash the partition table with esptool.py.

idf.py flash: Will flash everything including the partition table.