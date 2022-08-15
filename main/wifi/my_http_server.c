#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#include "esp_err.h"
#include "esp_log.h"

#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"


#include "my_http_server.h"

static const char *TAG = "http_server";

/* Max size of an individual file. Make sure this
 * value is same as that set in index.html */
#define MAX_FILE_SIZE   (200*1024) // 200 KB
#define MAX_FILE_SIZE_STR "200KB"

/* Scratch buffer size */
#define SCRATCH_BUFSIZE  4096

char buff[SCRATCH_BUFSIZE];

typedef struct {
    httpd_handle_t server_hdl;
} http_server_t;

static http_server_t *my_http_server = NULL;


/* Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) */
static const char* get_path_from_uri(char *dest, const char *uri, size_t destsize)
{
    size_t pathlen = strlen(uri);
    const char *quest = strchr(uri, '?');
    if (quest) {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash) {
        pathlen = MIN(pathlen, hash - uri);
    }
    strlcpy(dest, uri, pathlen + 1);
    return dest;
}

/* Handler to respond with an icon file embedded in flash.
 * Browsers expect to GET website icon at URI /favicon.ico.
 * This can be overridden by uploading file with same name */
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char favicon_ico_end[]   asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}

static esp_err_t index_get_handler(httpd_req_t *req)
{
    extern const unsigned char index_html_start[] asm("_binary_index_html_start");
    extern const unsigned char index_html_end[]   asm("_binary_index_html_end");

    const size_t response_size = (index_html_end - index_html_start);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index_html_start, response_size);
    return ESP_OK;
}

/* Handler to upload a file onto the server */
static esp_err_t ota_post_handler(httpd_req_t *req)
{
    char filepath[64];

    /* Skip leading "/ota" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = get_path_from_uri(filepath,req->uri + sizeof("/ota") - 1, sizeof(filepath));
    if (!filename) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE(TAG, "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    /* File cannot be larger than a limit */
    if (req->content_len > MAX_FILE_SIZE) {
        ESP_LOGE(TAG, "File too large : %d bytes", req->content_len);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "File size must be less than "
                            MAX_FILE_SIZE_STR "!");
        /* Return failure to close underlying connection else the
         * incoming file content will keep the socket busy */
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Receiving file : %s...", filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    int received;

    /* Content length of the request gives
     * the size of the file being uploaded */
    int remaining = req->content_len;

    while (remaining > 0) {

        ESP_LOGI(TAG, "Remaining size : %d", remaining);
        /* Receive the file part by part into a buffer */
        if ((received = httpd_req_recv(req, buff, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry if timeout occurred */
                continue;
            }

            // todo clean stuff

            ESP_LOGE(TAG, "File reception failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }

//        /* Write buffer content to file on storage */
//        if (received && (received != fwrite(buf, 1, received, fd))) {
//            /* Couldn't write everything to file!
//             * Storage may be full? */
//            fclose(fd);
//            unlink(filepath);
//
//            ESP_LOGE(TAG, "File write failed!");
//            /* Respond with 500 Internal Server Error */
//            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
//            return ESP_FAIL;
//        }

        /* Keep track of remaining size of
         * the file left to be uploaded */
        remaining -= received;
    }

    /* Close file upon upload completion */
    ESP_LOGI(TAG, "File reception complete");

    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "File uploaded successfully");
    return ESP_OK;
}


esp_err_t my_http_server_start() {
    if (my_http_server) {
        ESP_LOGE(TAG, "Http server already started");
        return ESP_ERR_INVALID_STATE;
    }

    /* Allocate memory for server data */
    my_http_server = calloc(1, sizeof(http_server_t));
    if (!my_http_server) {
        ESP_LOGE(TAG, "Failed to allocate memory for server data");
        return ESP_ERR_NO_MEM;
    }

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /* Use the URI wildcard matching function in order to
     * allow the same handler to respond to multiple different
     * target URIs which match the wildcard scheme */
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting HTTP Server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start file server!");
        return ESP_FAIL;
    }

    my_http_server->server_hdl = server;

    httpd_uri_t favicon = {
            .uri       = "/favicon.ico",  // Match all URIs of type /path/to/file
            .method    = HTTP_GET,
            .handler   = favicon_get_handler,
            .user_ctx  = my_http_server    // Pass server data as context
    };
    httpd_register_uri_handler(server, &favicon);

    httpd_uri_t index = {
            .uri       = "/index.html",  // Match all URIs of type /path/to/file
            .method    = HTTP_GET,
            .handler   = index_get_handler,
            .user_ctx  = my_http_server    // Pass server data as context
    };
    httpd_register_uri_handler(server, &index);

    /* URI handler for uploading files to server */
    httpd_uri_t ota = {
            .uri       = "/ota",   // Match all URIs of type /upload/path/to/file
            .method    = HTTP_POST,
            .handler   = ota_post_handler,
            .user_ctx  = my_http_server    // Pass server data as context
    };
    httpd_register_uri_handler(server, &ota);

    return ESP_OK;
}

esp_err_t my_http_server_stop() {
    if (!my_http_server) {
        ESP_LOGE(TAG, "Http server not started");
        return ESP_ERR_INVALID_STATE;
    }

    httpd_stop(my_http_server->server_hdl);
    free(my_http_server);
    my_http_server = NULL;

    ESP_LOGI(TAG, "Http server stopped.");

    return ESP_OK;
}