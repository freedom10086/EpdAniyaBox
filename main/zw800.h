#ifndef ZW800_H
#define ZW800_H

#include "esp_types.h"
#include "esp_event.h"
#include "esp_err.h"
#include "driver/uart.h"

#define CONFIG_ZW800_UART_RXD 48
#define CONFIG_ZW800_UART_TXD 47
#define CONFIG_ZW800_TOUCH 45

#define ZW800_CONFIG_DEFAULT()                              \
    {                                                       \
        .uart = {                                           \
            .uart_port = UART_NUM_2,                        \
            .rx_pin = CONFIG_ZW800_UART_RXD,                \
            .tx_pin = CONFIG_ZW800_UART_TXD,                \
            .baud_rate = 57600,                             \
            .data_bits = UART_DATA_8_BITS,                  \
            .parity = UART_PARITY_DISABLE,                  \
            .stop_bits = UART_STOP_BITS_1,                  \
        },                                                  \
        .touch_pin = CONFIG_ZW800_TOUCH,                    \
    }

typedef struct {
    int touch_pin;
    struct {
        uart_port_t uart_port;        /*!< UART port number */
        int rx_pin;                  /*!< UART Rx Pin number */
        int tx_pin;
        uint32_t baud_rate;           /*!< UART baud rate */
        uart_word_length_t data_bits; /*!< UART data bits length */
        uart_parity_t parity;         /*!< UART parity */
        uart_stop_bits_t stop_bits;   /*!< UART stop bits length */
    } uart;                           /*!< UART specific configuration */
} zw800_config_t;

esp_err_t zw800_init(const zw800_config_t *config, esp_event_loop_handle_t event_loop_hdl);

#endif