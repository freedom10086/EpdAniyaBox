#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "zw800.h"

#define TAG "zw800"

#define BUF_SIZE (512)
#define READ_RESPONSE_TIME_OUT 50

/**
 * pin 1-6
 * 1. V_TOUCH 3.3始终供电
 * 2. TouchOut 高电平有触摸 休眠后恢复低电平
 * 3. VCC
 * 4. Tx   波特率57600
 * 5. Rx
 * 6. GNd
 */
typedef struct {
    uint8_t *buff;
    uart_port_t uart_port;
    int touch_pin;
    esp_event_loop_handle_t event_loop_hdl;
    TaskHandle_t tsk_hdl;
} zw800_t;

/**
 * 命令包：
 * 包头(2)   芯片地址(4)  包标识(1) 包长度(2)   指令(1) 参数 1 … 参数 N 校验和(2)
 * 0xEF01  0XFF*4       0X01        X
 * 包长度x等于 指令（1） + 参数 + 校验和（2）
 *
 * 应答包：
 * 包头(2)   芯片地址(4)  包标识(1) 包长度(2)   确认码(1) 参数 1 … 参数 N 校验和(2)
 * 确认码：
 * 00H：表示指令执行完毕或 OK；
 * 01H：表示数据包接收错误；
 * 02H：表示传感器上没有手指；
 * 09H：表示没搜索到指纹；
 * 0aH：表示特征合并失败；
 * 0bH：表示访问指纹库时地址序号超出指纹库范围；
 * 10H：表示删除模板失败；
 * 11H：表示清空指纹库失败；
 * 13H：表示口令不正确；
 * 1eH：自动注册（enroll）失败；
 * 1fH：指纹库满；
 */
typedef struct {
    uint8_t data[16];
    uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} zw800_cmd_t;

uint8_t write_buf[20];
zw800_cmd_t ZW800_CMD_COMMON_HEAD = {{0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01}, 7};
zw800_cmd_t ZW800_CMD_SLEEP = {{0x33,}, 1};
zw800_cmd_t ZW800_CMD_ECHO = {{0x35,}, 1};
zw800_cmd_t ZW800_CMD_GET_IMAGE = {{0x01,}, 1}; // 验证用获取图像 =01H 表示收包有错, 02H 表示传感器上无手指
// 将获取到的原始图像生成指纹特征, 存到 参数1索引的charBuff CharBuffer1、CharBuffer2 、CharBuffer3，CharBuffer4,
zw800_cmd_t ZW800_CMD_PS_GetChar = {{0x02, 0x01}, 1};
// 以特征文件缓冲区中的特征文件搜索整个或部分指纹库 /* buffId, startPage pageNum */
zw800_cmd_t ZW800_CMD_PS_SearchM = {{0x02, 0x01, 0x00, 0x00, 0xFF, 0xFF}, 1};
zw800_cmd_t ZW800_CMD_PS_EMPTY = {{0x0D,}, 1}; // 清空指纹库
zw800_cmd_t ZW800_CMD_RESTORE_TO_DEFAULT = {{0x36,}, 1}; // 恢复出厂设置
// 读有效模板个数
zw800_cmd_t ZW800_CMD_PS_ValidTempleteNum = {{0x1D,}, 1};
// 自动注册 id号(2) 录入次数(1) 参数(2)
zw800_cmd_t ZW800_CMD_PS_AutoEnroll = {{0x31, '\0', '\0', 0x04, '\0', '\0'}, 6};

static zw800_t zw800;
static TaskHandle_t xTaskToNotify = NULL;
const UBaseType_t xArrayIndex = 0;

static void zw800_write_cmd(zw800_t *zw800_dev, zw800_cmd_t cmd) {
    uint16_t pack_len = cmd.databytes + 2;
    uint8_t len = ZW800_CMD_COMMON_HEAD.databytes + 2 + cmd.databytes + 2;

    // head
    memcpy(write_buf, ZW800_CMD_COMMON_HEAD.data, ZW800_CMD_COMMON_HEAD.databytes);
    // len
    write_buf[ZW800_CMD_COMMON_HEAD.databytes] = (pack_len >> 8) & 0xFF;
    write_buf[ZW800_CMD_COMMON_HEAD.databytes + 1] = pack_len & 0xFF;

    // cmd and param
    memcpy(&write_buf[ZW800_CMD_COMMON_HEAD.databytes + 2], cmd.data, cmd.databytes);

    // checksum
    uint16_t check_sum = 0x01; // 包标识为0x01
    for (int i = 0; i < 2 + cmd.databytes; ++i) {
        check_sum += write_buf[ZW800_CMD_COMMON_HEAD.databytes + i];
    }
    write_buf[ZW800_CMD_COMMON_HEAD.databytes + 2 + cmd.databytes] = (check_sum >> 8) & 0xFF;
    write_buf[ZW800_CMD_COMMON_HEAD.databytes + 2 + cmd.databytes + 1] = check_sum & 0xFF;

    uart_write_bytes(zw800_dev->uart_port, write_buf, len);

    ESP_LOGI(TAG, "write cmd to zw800 ");
            esp_log_buffer_hex(TAG, write_buf, len);

    vTaskDelay(pdMS_TO_TICKS(5));
}

static uint8_t zw800_read_response(zw800_t *zw800_dev) {
    int len = uart_read_bytes(zw800_dev->uart_port, zw800_dev->buff, (BUF_SIZE - 1),
                              READ_RESPONSE_TIME_OUT / portTICK_PERIOD_MS);
    if (len) {
        ESP_LOGI(TAG, "read from zw800 ");
                esp_log_buffer_hex(TAG, zw800_dev->buff, len);
        if (len >= 12 && zw800_dev->buff[6] == 0x07) {
            return zw800_dev->buff[9];
        }
    }
    return 0xFF;
}

static void IRAM_ATTR gpio_isr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t) arg;
    // xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* Notify the task that the transmission is complete. */
    vTaskNotifyGiveIndexedFromISR(xTaskToNotify, xArrayIndex, &xHigherPriorityTaskWoken);
}

int get_valid_template_num(zw800_t *zw800_dev) {
    zw800_write_cmd(zw800_dev, ZW800_CMD_PS_ValidTempleteNum);
    if (0x00 == zw800_read_response(zw800_dev)) {
        uint16_t cnt = (zw800_dev->buff[10] << 8 | zw800_dev->buff[11]);
        return cnt;
    }

    return -1;
}

int auro_enroll(zw800_t *zw800_dev, uint16_t page_id) {
    ZW800_CMD_PS_AutoEnroll.data[10] = page_id >> 8;
    ZW800_CMD_PS_AutoEnroll.data[11] = page_id & 0xff;
    ZW800_CMD_PS_AutoEnroll.data[12] = 0x00;
    ZW800_CMD_PS_AutoEnroll.data[13] = 0x00;
    zw800_write_cmd(zw800_dev, ZW800_CMD_PS_AutoEnroll);

    // 40s
    uint16_t max_wait_timeout_cnt = 40 * 1000 / READ_RESPONSE_TIME_OUT;
    uint16_t timeout_cnt = 0;

    while (1) {
        int response = zw800_read_response(zw800_dev);
        uint16_t response_len = zw800_dev->buff[7] << 8 | zw800_dev->buff[8];
        if (response != 0xff && response_len != 5) {
            // failed
            ESP_LOGW(TAG, "auto enroll failed, data len not exist response code: %x", response);
            return 0xFF;
        }

        // get 参数 1 & 2
        uint8_t stage = zw800_dev->buff[10];
        uint8_t param2 = zw800_dev->buff[11];

        if (response != 0x00 && response != 0xFF) {
            ESP_LOGW(TAG, "auto enroll failed, response: %x, param1: %x, param2: %x");
            return response;
        }

        if (response == 0xff) {
            // time out continue
            timeout_cnt++;
            if (timeout_cnt >= max_wait_timeout_cnt) {
                ESP_LOGW(TAG, "auto enroll time out, exit");
                // TODO send cmd cancel
                return 0xFF;
            }
            continue;
        }

        timeout_cnt = 0;
        if (stage == 0x00) {
            // success enter
            ESP_LOGI(TAG, "success enter auto enroll mode, pageId:%d", page_id);
        } else if (stage == 0x01) {
            ESP_LOGI(TAG, "auto enroll get image, times: %d", param2);
        } else if (stage == 0x02) {
            ESP_LOGI(TAG, "auto enroll gen char, times: %d", param2);
        } else if (stage == 0x03) {
            ESP_LOGI(TAG, "auto enroll finger leave, times: %d", param2);
        } else if (stage == 0x04) {
            ESP_LOGI(TAG, "auto enroll combine, times: %d", param2);
        } else if (stage == 0x05 && param2 == 0xf1) {
            ESP_LOGI(TAG, "auto enroll reg verify if exist");
        } else if (stage == 0x06 && param2 == 0xf2) {
            // 模板存储结果
            ESP_LOGI(TAG, "auto enroll save result success, saved to page_id %d, exit auto mode", page_id);
            return response;
        }
    }
}

static void zw800_task_entry(void *arg) {
    zw800_t *zw800_dev = (zw800_t *) arg;
    xTaskToNotify = xTaskGetCurrentTaskHandle();

    ESP_LOGI(TAG, "configTASK_NOTIFICATION_ARRAY_ENTRIES %d", configTASK_NOTIFICATION_ARRAY_ENTRIES);

    while (1) {
        while (1) {
            if (!gpio_get_level(zw800_dev->touch_pin)) {
                ESP_LOGI(TAG, "in sleep mode");

                /* Wait to be notified that the transmission is complete.  Note the first parameter is pdTRUE, which has the effect of clearing
                the task's notification value back to 0, making the notification value act like a binary (rather than a counting) semaphore.  */
                uint32_t ulNotificationValue = ulTaskNotifyTakeIndexed(xArrayIndex,
                                                                      pdTRUE,
                                                                      pdMS_TO_TICKS(3000));
                ESP_LOGI(TAG, "ulTaskGenericNotifyTake %d", ulNotificationValue);
                if (ulNotificationValue == 1) {
                    /* The transmission ended as expected. */
                    gpio_isr_handler_remove(zw800_dev->touch_pin);
                } else {
                    /* The call to ulTaskNotifyTake() timed out. */
                }
            } else {
                break;
            }
        }

        zw800_write_cmd(zw800_dev, ZW800_CMD_ECHO);
        uint8_t response = zw800_read_response(zw800_dev);
        if (response != 0x00) {
            ESP_LOGE(TAG, "hand shake with zw800 failed, response code: %x", response);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // check template num
        int temp_num = get_valid_template_num(zw800_dev);
        ESP_LOGI(TAG, "zw800 template num: %d", temp_num);
        if (temp_num == 0) {
            // enter auto enroll mode
            auro_enroll(zw800_dev, temp_num + 1);
        }

        zw800_write_cmd(zw800_dev, ZW800_CMD_SLEEP);
        response = zw800_read_response(zw800_dev);
        if (response != 0x00) {
            ESP_LOGE(TAG, "enter sleep mode failed", response);
        } else {
            gpio_isr_handler_add(zw800_dev->touch_pin, gpio_isr_handler, (void *) zw800_dev->touch_pin);
            ESP_LOGI(TAG, "zw800 enter sleep mode");
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelete(NULL);
}

esp_err_t zw800_init(const zw800_config_t *config, esp_event_loop_handle_t event_loop_hdl) {
    /* Install UART friver */
    uart_config_t uart_config = {
            .baud_rate = config->uart.baud_rate,
            .data_bits = config->uart.data_bits,
            .parity = config->uart.parity,
            .stop_bits = config->uart.stop_bits,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_APB,
    };

    zw800.uart_port = config->uart.uart_port;

    // config touch detect isr gpio pin
    zw800.touch_pin = config->touch_pin;
    // setup gpio
    ESP_LOGI(TAG, "touch pin is %d", config->touch_pin);
    gpio_config_t io_config = {
            .pin_bit_mask = (1ull << config->touch_pin),
            .mode = GPIO_MODE_INPUT,
            .intr_type = GPIO_INTR_POSEDGE, //interrupt of rising edge GPIO_INTR_HIGH_LEVEL
            .pull_up_en = 0,
            .pull_down_en = 1,
    };
    ESP_ERROR_CHECK(gpio_config(&io_config));

    //install gpio isr service
    gpio_install_isr_service(0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(config->touch_pin, gpio_isr_handler, (void *) config->touch_pin);

    if (uart_driver_install(zw800.uart_port, 1024, 0,
                            0, NULL, 0) != ESP_OK) {
        ESP_LOGE(TAG, "install uart driver failed");
        goto err_uart_install;
    }
    if (uart_param_config(zw800.uart_port, &uart_config) != ESP_OK) {
        ESP_LOGE(TAG, "config uart parameter failed");
        goto err_uart_config;
    }
    if (uart_set_pin(zw800.uart_port, config->uart.tx_pin, config->uart.rx_pin,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        ESP_LOGE(TAG, "config uart gpio failed");
        goto err_uart_config;
    }
    zw800.buff = calloc(1, BUF_SIZE);
    if (!zw800.buff) {
        ESP_LOGE(TAG, "calloc memory for runtime buffer failed");
        goto err_buffer;
    }

    zw800.event_loop_hdl = event_loop_hdl;

    /* Create NMEA Parser task */
    BaseType_t err = xTaskCreate(
            zw800_task_entry,
            "zw800_task",
            3072,
            &zw800,
            uxTaskPriorityGet(NULL),
            &zw800.tsk_hdl);
    if (err != pdTRUE) {
        ESP_LOGE(TAG, "create zw800 task failed");
        goto err_task_create;
    }
    ESP_LOGI(TAG, "zw800 task init OK");
    return ESP_OK;
    /*Error Handling*/
    err_task_create:
    err_uart_install:
    uart_driver_delete(zw800.uart_port);
    err_uart_config:
    err_buffer:
    free(zw800.buff);
    return ESP_FAIL;
}