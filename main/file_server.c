/* HTTP File Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "file_server.h"

#include "log.h"
#include "serial.h"

#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#include <esp_err.h>
#include <esp_log.h>

#include <esp_vfs.h>
#include <esp_http_server.h>
#include <driver/uart.h>

#define ECHO_UART_PORT_NUM (2)

void Console_Toggle_LidSwitch();
void Console_Toggle_Power();
void Console_Reset();

typedef struct file_server_data
{
    // Base path of file storage
    char base_path[1024 + 1];

    // Scratch buffer for temporary storage during file transfer
    char scratch[8192];
} file_server_data;

// Handler to redirect incoming GET request for /index.html to
// This can be overridden by uploading file with same name
static esp_err_t index_html_get_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0); // Response body can be empty
    return ESP_OK;
}

// Send HTTP response with a run-time generated html consisting of
// a list of all files and folders under the requested path.
// In case of SPIFFS this returns empty list when path is any
// string other than '/', since SPIFFS doesn't support directories
static esp_err_t http_resp_dir_html(httpd_req_t *req, const char *dirpath)
{
    // Send HTML file header
    httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><body>");

    /* Send file-list table definition and column labels */
    httpd_resp_sendstr_chunk(req,
                             "<h1>PS1 PSU</h1><br>");
    httpd_resp_sendstr_chunk(req,
                             "<form action='/reset/'><input type='submit' value='RESET'/></form>");
    httpd_resp_sendstr_chunk(req,
                             "<form action='/power/'><input type='submit' value='TOGGLE POWER'/></form>");
    httpd_resp_sendstr_chunk(req,
                             "<form action='/lid/'><input type='submit' value='TOGGLE LID'/></form>");
    httpd_resp_sendstr_chunk(req,
                             "<form action='/fast/'><input type='submit' value='FAST'/></form>");
    httpd_resp_sendstr_chunk(req,
                             "<form action='/slow/'><input type='submit' value='SLOW'/></form>");
    httpd_resp_sendstr_chunk(req,
                             "<form action='/toggleserial/'><input type='submit' value='Toggle Serial'/></form>");
    httpd_resp_sendstr_chunk(req,
                             "<label>Serial:</label>");
    if (uart_is_driver_installed(ECHO_UART_PORT_NUM))
    {
        httpd_resp_sendstr_chunk(req,
                                 "<input type='text' value='ON' disabled/>");
    }
    else
    {
        httpd_resp_sendstr_chunk(req,
                                 "<input type='text' value='OFF' disabled/>");
    }

    /* Send remaining chunk of HTML file to complete it */
    httpd_resp_sendstr_chunk(req, "</body></html>");

    /* Send empty chunk to signal HTTP response completion */
    httpd_resp_sendstr_chunk(req, NULL);

    return ESP_OK;
}

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

/* Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) */
static const char *get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest)
    {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash)
    {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize)
    {
        /* Full path string won't fit into destination buffer */
        return NULL;
    }

    /* Construct full path (base + path) */
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}

/* Handler to download a file kept on the server */
static esp_err_t download_get_handler(httpd_req_t *req)
{
    char filepath[1024];

    const char *filename = get_path_from_uri(filepath, ((file_server_data *)req->user_ctx)->base_path,
                                             req->uri, sizeof(filepath));
    if (!filename)
    {
        ESP_LOGE(kLogPrefix, "Filename is too long");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    if (strcmp(filename, "/index.html") == 0)
    {
        return index_html_get_handler(req);
    }
    else if (strcmp(filename, "/power/") == 0)
    {
        Console_Toggle_Power();
        Serial_Slow();
        ESP_LOGI(kLogPrefix, "Got power toggle request!\n");
        return index_html_get_handler(req);
    }
    else if (strcmp(filename, "/lid/") == 0)
    {
        Console_Toggle_LidSwitch();
        ESP_LOGI(kLogPrefix, "Got lid toggle request!\n");
        return index_html_get_handler(req);
    }
    else if (strcmp(filename, "/reset/") == 0)
    {
        Console_Reset();
        Serial_Slow();
        ESP_LOGI(kLogPrefix, "Got reset request!\n");
        return index_html_get_handler(req);
    }
    else if (strcmp(filename, "/fast/") == 0)
    {
        Serial_Fast();
        ESP_LOGI(kLogPrefix, "Got fast request!\n");
        return index_html_get_handler(req);
    }
    else if (strcmp(filename, "/slow/") == 0)
    {
        Serial_Slow();
        ESP_LOGI(kLogPrefix, "Got slow request!\n");
        return index_html_get_handler(req);
    }
    else if (strcmp(filename, "/toggleserial/") == 0)
    {
        Serial_Toggle();
        ESP_LOGI(kLogPrefix, "Got toggle serial request!\n");
        return index_html_get_handler(req);
    }
    else if (filename[strlen(filename) - 1] == '/') // If name has trailing '/', respond with directory contents
    {
        return http_resp_dir_html(req, filepath);
    }

    return ESP_OK;
}

/* Function to start the file server */
esp_err_t start_file_server(const char *base_path)
{
    static struct file_server_data *server_data = NULL;

    if (server_data)
    {
        ESP_LOGE(kLogPrefix, "File server already started");
        return ESP_ERR_INVALID_STATE;
    }

    /* Allocate memory for server data */
    server_data = (file_server_data *)calloc(1, sizeof(file_server_data));
    if (!server_data)
    {
        ESP_LOGE(kLogPrefix, "Failed to allocate memory for server data");
        return ESP_ERR_NO_MEM;
    }
    strlcpy(server_data->base_path, base_path,
            sizeof(server_data->base_path));

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Use the URI wildcard matching function in order to
    // allow the same handler to respond to multiple different
    // target URIs which match the wildcard scheme
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(kLogPrefix, "Starting HTTP Server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK)
    {
        ESP_LOGE(kLogPrefix, "Failed to start file server!");
        return ESP_FAIL;
    }

    // URI handler for getting uploaded files
    httpd_uri_t file_download = {
        .uri = "/*", // Match all URIs of type /path/to/file
        .method = HTTP_GET,
        .handler = download_get_handler,
        .user_ctx = server_data // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_download);

    return ESP_OK;
}