/* HTTP File Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "file_server.h"

#include "bridge/bridge.h"
#include "system/log.h"
#include "tty/serial.h"
#include "tty/sio1.h"
#include "system/pins.h"

#include <driver/uart.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>

#define ECHO_UART_PORT_NUM (2)

void Toggle_DTR();
void Toggle_RTS();

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
    char buffer[32];

    // Send HTML chunk
    httpd_resp_sendstr_chunk(req,
                             "<!DOCTYPE html>"
                             "<html>"
                             "  <style>table, th, td {border:1px solid black;}</style>"
                             "  <body>"
                             "      <h1>PS1 LINK THING</h1>"
                             "      <table>"
                             "          <tr><th></th><th>Pin</th><th>State</th></tr>");

    // Send DSR
    httpd_resp_sendstr_chunk(req, "<tr><td></td><td>DSR</td><td>");
    if (gpio_get_level(kPin_DSR))
    {
        httpd_resp_sendstr_chunk(req, "High</td></tr>");
    }
    else
    {
        httpd_resp_sendstr_chunk(req, "Low</td></tr>");
    }

    // Send CTS
    httpd_resp_sendstr_chunk(req, "<tr><td></td><td>CTS</td><td>");
    if (gpio_get_level(kPin_CTS))
    {
        httpd_resp_sendstr_chunk(req, "High</td></tr>");
    }
    else
    {
        httpd_resp_sendstr_chunk(req, "Low</td></tr>");
    }

    // Send DTR
    httpd_resp_sendstr_chunk(req, "<tr><td><form action='/dtr/'><input type='submit' value='Toggle'/></form></td><td>DTR</td><td>");
    if (dtr_state)
    {
        httpd_resp_sendstr_chunk(req, "High</td></tr>");
    }
    else
    {
        httpd_resp_sendstr_chunk(req, "Low</td></tr>");
    }

    // Send RTS
    httpd_resp_sendstr_chunk(req, "<tr><td><form action='/rts/'><input type='submit' value='Toggle'/></form></td><td>RTS</td><td>");
    if (rts_state)
    {
        httpd_resp_sendstr_chunk(req, "High</td></tr>");
    }
    else
    {
        httpd_resp_sendstr_chunk(req, "Low</td></tr>");
    }

    // Upload status
    httpd_resp_sendstr_chunk(req, "<tr><td><form action='/upload/'><input type='submit' value='Toggle'/></form></td><td>Upload</td><td>");
    if (disable_uploads)
    {
        httpd_resp_sendstr_chunk(req, "Disabled");
    }
    else
    {
        httpd_resp_sendstr_chunk(req, "Enabled");
    }

    // UUART Settings
    httpd_resp_sendstr_chunk(req,
                             "          </td></tr>"
                             "      </table><br><br>"
                             "      <b>UART Settings</b>"
                             "      <form action='/'>"
                             "          <table>"
                             "              <tr><th>Setting</th><th>Value</th></tr>"
                             "              <tr><td>Baud</td><td><input type='number' id='baud' name='baud' min='1' max='2073600' value='");

    // Send baud rate
    sprintf(buffer, "%i", uart_config.baud_rate);
    httpd_resp_sendstr_chunk(req, buffer);
    httpd_resp_sendstr_chunk(req, "'></td></tr>"
                                  "              <tr><td>Parity</td><td><select name='parity' id='parity' ");

    if (uart_config.parity == UART_PARITY_DISABLE)
        httpd_resp_sendstr_chunk(req, "selected");

    httpd_resp_sendstr_chunk(req,
                             "><option value='disable'>Disable</option><option value='even' ");

    if (uart_config.parity == UART_PARITY_EVEN)
        httpd_resp_sendstr_chunk(req, "selected");

    httpd_resp_sendstr_chunk(req,
                             ">Even</option><option value='odd'");

    if (uart_config.parity == UART_PARITY_ODD)
        httpd_resp_sendstr_chunk(req, "selected");

    httpd_resp_sendstr_chunk(req,
                             ">Odd</option></select></td></tr>"
                             "              <tr><td>Stop bits</td><td><select name='stop_bits' id='stop_bits'><option value='1' ");

    if (uart_config.stop_bits == UART_STOP_BITS_1)
        httpd_resp_sendstr_chunk(req, "selected");

    httpd_resp_sendstr_chunk(req, ">1</option><option value='1.5' ");

    if (uart_config.stop_bits == UART_STOP_BITS_1_5)
        httpd_resp_sendstr_chunk(req, "selected");

    httpd_resp_sendstr_chunk(req, ">1.5</option><option value='2' ");

    if (uart_config.stop_bits == UART_STOP_BITS_2)
        httpd_resp_sendstr_chunk(req, "selected");

    httpd_resp_sendstr_chunk(req,
                             ">2</option></select></td></tr>"
                             "          </table>"
                             "          <input type='submit' value='Set'/>"
                             "      </form>"
                             "  </body>"
                             "</html>");

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
    unsigned long new_baud;

    char *buf;
    char param[32];
    int buf_len;

    const char *filename = get_path_from_uri(filepath, ((file_server_data *)req->user_ctx)->base_path,
                                             req->uri, sizeof(filepath));
    if (!filename)
    {
        ESP_LOGE(kLogPrefix, "Filename is too long");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    buf_len = httpd_req_get_url_query_len(req) + 1;

    if (buf_len > 1)
    {
        buf = (char *)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            ESP_LOGI(kLogPrefix, "Found URL query => %s", buf);

            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "baud", param, sizeof(param)) == ESP_OK)
            {
                ESP_LOGI(kLogPrefix, "Found URL query parameter => baud=%s", param);
                new_baud = strtoul(param, NULL, 0);

                if (new_baud > 0)
                {
                    // Set new baud rate
                    ESP_LOGI(kLogPrefix, "Setting baud rate to %li", new_baud);
                    uart_config.baud_rate = new_baud;
                }
                else
                {
                    ESP_LOGE(kLogPrefix, "Invalid baud : %s", param);
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid baud parameter");
                    return ESP_FAIL;
                }
            }
            
            if (httpd_query_key_value(buf, "parity", param, sizeof(param)) == ESP_OK)
            {
                if(strcmp(param, "disable") == 0)
                {
                    ESP_LOGI(kLogPrefix, "Setting parity to disable");
                    uart_config.parity = UART_PARITY_DISABLE;
                }
                else if(strcmp(param, "even") == 0)
                {
                    ESP_LOGI(kLogPrefix, "Setting parity to even");
                    uart_config.parity = UART_PARITY_EVEN;
                }
                else if(strcmp(param, "odd") == 0)
                {
                    ESP_LOGI(kLogPrefix, "Setting parity to odd");
                    uart_config.parity = UART_PARITY_ODD;
                }
                else
                {
                    ESP_LOGE(kLogPrefix, "Invalid parity : %s", param);
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid parity parameter");
                    return ESP_FAIL;
                }
            }
            
            if (httpd_query_key_value(buf, "stop_bits", param, sizeof(param)) == ESP_OK)
            {
                if(strcmp(param, "1") == 0)
                {
                    ESP_LOGI(kLogPrefix, "Setting stop bits to 1");
                    uart_config.stop_bits = UART_STOP_BITS_1;
                }
                else if(strcmp(param, "1.5") == 0)
                {
                    ESP_LOGI(kLogPrefix, "Setting stop bits to 1.5");
                    uart_config.stop_bits = UART_STOP_BITS_1_5;
                }
                else if(strcmp(param, "2") == 0)
                {
                    ESP_LOGI(kLogPrefix, "Setting stop bits to 2");
                    uart_config.stop_bits = UART_STOP_BITS_2;
                }
                else
                {
                    ESP_LOGE(kLogPrefix, "Invalid stop bits : %s", param);
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid stop bits parameter");
                    return ESP_FAIL;
                }
            }
            else
            {
                ESP_LOGE(kLogPrefix, "Invalid parameter : %s", param);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid parameter");
                return ESP_FAIL;
            }
        }
        ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));

        free(buf);
        return index_html_get_handler(req);
    }

    if (strcmp(filename, "/index.html") == 0)
    {
        return index_html_get_handler(req);
    }
    else if (strcmp(filename, "/dtr/") == 0)
    {
        Toggle_DTR();
        ESP_LOGI(kLogPrefix, "Got DTR toggle request!\n");
        return index_html_get_handler(req);
    }
    else if (strcmp(filename, "/rts/") == 0)
    {
        Toggle_RTS();
        ESP_LOGI(kLogPrefix, "Got RTS toggle request!\n");
        return index_html_get_handler(req);
    }
    else if (strcmp(filename, "/upload/") == 0)
    {
        disable_uploads = !disable_uploads;
        ESP_LOGI(kLogPrefix, "Uploading toggled!\n");
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