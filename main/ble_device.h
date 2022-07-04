#ifndef __BLE_DEVICE_H
#define __BLE_DEVICE_H

#include "esp_event_base.h"
#include "esp_types.h"
#include "esp_event.h"
#include "esp_err.h"
#include "driver/uart.h"

ESP_EVENT_DECLARE_BASE(ESP_BLE_DEVICE_EVENT);

typedef struct {
    struct {
        uart_port_t uart_port;        /*!< UART port number */
        uint32_t rx_pin;              /*!< UART Rx Pin number */
        uint32_t baud_rate;           /*!< UART baud rate */
        uart_word_length_t data_bits; /*!< UART data bits length */
        uart_parity_t parity;         /*!< UART parity */
        uart_stop_bits_t stop_bits;   /*!< UART stop bits length */
        uint32_t event_queue_size;    /*!< UART event queue size */
    } uart;                           /*!< UART specific configuration */
} ble_device_config_t;

esp_event_loop_handle_t ble_device_init(const ble_device_config_t *config);

esp_err_t ble_device_deinit(esp_event_loop_handle_t hdl);

esp_err_t ble_device_add_handler(esp_event_loop_handle_t hdl, esp_event_handler_t event_handler, void *handler_args);

esp_err_t ble_device_remove_handler(esp_event_loop_handle_t hdl, esp_event_handler_t event_handler);

#endif