file(GLOB_RECURSE SOURCE_FILES *.c *.cc *.S)
idf_component_register(
	SRCS ${SOURCE_FILES}
    INCLUDE_DIRS .
    REQUIRES esp_http_server esp_wifi nvs_flash)
