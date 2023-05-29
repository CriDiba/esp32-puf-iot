#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define STR_CHALL_MAX_LEN 20

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define NEWLINE "\r\n"

typedef struct {
    size_t length;
    char *string;
} String;

typedef struct {
    size_t length;
    const char *string;
} CString;

#define mkCSTRING(s) ((CString){.string = (s), .length = (sizeof(s) - 1)})

typedef struct {
    size_t length;
    uint8_t *buffer;
} Buffer;

typedef struct {
    size_t length;
    const uint8_t *buffer;
} CBuffer;