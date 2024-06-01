//
// Created by yang on 2024/5/21.
//

#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include <dirent.h>
#include "wifi/my_file_server_common.h"
#include "sys/stat.h"
#include "dictionary_page.h"
#include "page_manager.h"

#define TAG "dictionary-page"

/**
 * dict 定义
 * 单词｜音标｜中文释义\n
 *
 */

RTC_DATA_ATTR static int32_t current_dic_index = 0;
RTC_DATA_ATTR static int32_t max_dic_index = 9999999;

static bool file_system_mounted = false;

static long find_line_start(FILE *file, int n) {
    // 检查输入是否有效
    if (file == NULL || n < 0) {
        return -1;
    }

    if (n == 0) {
        return 0;
    }

    // read index file
    char file_path[32];
    /* Retrieve the base path of file storage to construct the full path */
    strcpy(file_path, FILE_SERVER_BASE_PATH);
    strlcpy(file_path + strlen(FILE_SERVER_BASE_PATH), "/dict_index",
            sizeof(file_path) - strlen(FILE_SERVER_BASE_PATH));

    struct stat entry_stat;
    if (stat(file_path, &entry_stat) == 0) { // index file exist
        // draw dict
        FILE *index_file = fopen(file_path, "r");
        if (index_file == NULL) {
            perror("Failed to open file");
            return -1;
        }

        max_dic_index = entry_stat.st_size / 2;
        ESP_LOGI(TAG, "Found %s (%ld bytes) max dic:%ld", file_path, entry_stat.st_size, max_dic_index);

        // one index two byte
        uint16_t dest_index = n * 2;
        if (dest_index > entry_stat.st_size) {
            return -1;
        }

        fseek(index_file, dest_index, SEEK_SET);

        uint8_t buff[2];
        size_t readed = fread(buff, 1, 2, index_file);

        fclose(index_file);

        if (readed < 2) {
            return -1;
        }
        long line_start_pos = buff[0] << 8 | buff[1];
        return line_start_pos;
    } else {
        ESP_LOGE(TAG, "no dict index file %s", file_path);
        return -1;
    }
}

void dictionary_page_on_create(void *arg) {
    esp_err_t err = mount_storage(FILE_SERVER_BASE_PATH, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mount failed %s", FILE_SERVER_BASE_PATH);
        file_system_mounted = false;
    } else {
        file_system_mounted = true;
    }
}

void dictionary_page_draw(epd_paint_t *epd_paint, uint32_t loop_cnt) {
    epd_paint_clear(epd_paint, 0);

    // read from file
    char file_path[32];
    /* Retrieve the base path of file storage to construct the full path */
    strcpy(file_path, FILE_SERVER_BASE_PATH);
    strlcpy(file_path + strlen(FILE_SERVER_BASE_PATH), "/dict.csv", sizeof(file_path) - strlen(FILE_SERVER_BASE_PATH));

    ESP_LOGI(TAG, "open dict file %s", file_path);

    struct stat entry_stat;
    if (stat(file_path, &entry_stat) == -1) {
        ESP_LOGE(TAG, "Failed to stat %s", file_path);
        return;
    }

    static char entrysize[16];
    sprintf(entrysize, "%ld", entry_stat.st_size);
    ESP_LOGI(TAG, "Found %s (%s bytes)", file_path, entrysize);

    // draw dict
    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file");
        return;
    }

    long pos = find_line_start(file, current_dic_index);
    if (pos == -1) {
        ESP_LOGE(TAG, "Could not find the start of line %ld\n", current_dic_index);
    } else {
        ESP_LOGI(TAG, "The start of line %ld is at position %ld\n", current_dic_index, pos);
        // 将文件指针设置到第n行的开头
        fseek(file, pos, SEEK_SET);
        // 从第n行开始读取内容
        static char buffer[256];
        if (fgets(buffer, sizeof(buffer), file) != NULL) {
            // find | and replace to 0
            uint16_t idx = 0;
            uint16_t total_len = strlen(buffer);
            uint16_t phonetic_idx = 0, cn_translate = 0;

            for (; idx < total_len; idx++) {
                if (buffer[idx] == '|') {
                    buffer[idx] = 0;
                    if (phonetic_idx == 0) {
                        phonetic_idx = idx + 1;
                    } else if (cn_translate == 0) {
                        cn_translate = idx + 1;
                    }
                }
            }

            epd_paint_draw_string_at(epd_paint, 0, 0, (char *) buffer, &Font20, 1);
            if (cn_translate > 0 && cn_translate < total_len) {
                epd_paint_draw_string_at_area(epd_paint, 0, 22, epd_paint->width, epd_paint->height,
                                              (char *) (buffer + cn_translate), &Font_HZK16, 1);
            }
        }
    }

    fclose(file);
}

bool dictionary_page_key_click(key_event_id_t key_event_type) {
    if (KEY_1_SHORT_CLICK == key_event_type && current_dic_index < max_dic_index) {
        current_dic_index++;
        page_manager_request_update(false);
        return true;
    } else if (KEY_2_SHORT_CLICK == key_event_type && current_dic_index > 0) {
        current_dic_index--;
        page_manager_request_update(false);
        return true;
    }
    return false;
}

void dictionary_page_on_destroy(void *arg) {
    if (file_system_mounted) {
        unmount_storage();
        file_system_mounted = false;
    }
}

int dictionary_page_on_enter_sleep(void *arg) {
    return DEFAULT_SLEEP_TS;
}