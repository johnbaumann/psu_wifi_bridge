#include "fifo.h"

bool cb_init(CircularBuffer* cb) {
    cb->start = 0;
    cb->end = 0;
    cb->size = CB_BUF_SIZE;
    return true;
}

void cb_reset(CircularBuffer* cb) {
    cb->start = 0;
    cb->end = 0;
    cb->size = CB_BUF_SIZE;
}

size_t cb_size(CircularBuffer* cb) {
    return cb->start - cb->end;
}

void cb_read(CircularBuffer* cb, uint8_t* dst) {
    if (cb->start == cb->end) {
        return;
    }
    *dst = cb->buffer[cb->end];
    cb->buffer[cb->end] = 0;
    cb->end = (cb->end + 1) % cb->size;
}

void cb_write(CircularBuffer* cb, uint8_t* src) {
    if ((cb->start - cb->end) == cb->size) {
        return;
    }
    cb->buffer[cb->start % cb->size] = *src;
    cb->start++;
}

int cb_readbytes(CircularBuffer* cb, uint8_t* dst, size_t size) {
    if (cb->start == cb->end) {
        return -1;
    }
    int count = 0;
    do {
        if (cb->start == cb->end) {
            break;
        }
        *dst = cb->buffer[cb->end];
        cb->buffer[cb->end] = 0;
        cb->end = (cb->end + 1) % cb->size;
        dst++;
        count++;
    } while (--size);

    return count;
}

int cb_writebytes(CircularBuffer* cb, uint8_t* src, size_t size) {
    if ((cb->start - cb->end) == cb->size) {
        return -1;
    }
    int count = 0;
    do {
        cb->buffer[cb->start % cb->size] = *src;
        cb->start++;
        src++;
        count++;
    } while (--size);

    return count;
}