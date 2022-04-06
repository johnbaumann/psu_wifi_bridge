#pragma once

#include <esp_err.h>

extern bool disable_uploads;

esp_err_t start_file_server(const char *base_path);
