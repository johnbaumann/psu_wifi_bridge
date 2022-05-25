#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#define CB_BUF_SIZE (1024 * 10)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t size;
    uint8_t start, end;
    uint8_t buffer[CB_BUF_SIZE];
} CircularBuffer;

void cb_reset(CircularBuffer* cb);

bool cb_init(CircularBuffer* cb);

size_t cb_size(CircularBuffer* cb);

void cb_read(CircularBuffer* cb, uint8_t* dst);

void cb_write(CircularBuffer* cb, uint8_t * src);

int cb_readbytes(CircularBuffer* cb, uint8_t* dst, size_t size);

int cb_writebytes(CircularBuffer* cb, uint8_t * src, size_t size);

#ifdef __cplusplus
} /* extern "C" */
#endif