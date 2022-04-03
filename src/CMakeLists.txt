idf_component_register(
	SRCS "main.cc"
         "bridge/bridge.cc"
         "network/file_server.cc"
         "network/siopayload.pb.c"
         "network/tcp.cc"
         "network/wifi_client.cc"
         "system/log.cc"
         "system/pins.cc"
         "tty/serial.cc"
         "tty/sio1.cc"
         "nanopb/pb_common.c"
         "nanopb/pb_decode.c"
         "nanopb/pb_encode.c"
    INCLUDE_DIRS .
    REQUIRES esp_http_server esp_wifi nvs_flash)