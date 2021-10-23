#ifndef _FILE_SERVER_H
#define _FILE_SERVER_H

#include <esp_err.h>

esp_err_t start_file_server(const char *base_path);

#endif // _FILE_SERVER_H