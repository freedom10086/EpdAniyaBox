#pragma once

#include "sdkconfig.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t mount_storage(const char *base_path);

esp_err_t unmount_storage();

esp_err_t register_file_server(const char *base_path, httpd_handle_t server);

esp_err_t unregister_file_server(httpd_handle_t server);

#ifdef __cplusplus
}
#endif
