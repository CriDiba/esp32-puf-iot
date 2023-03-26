//
// Ondrej Stanicek
// staniond@fit.cvut.cz
// Czech Technical University - Faculty of Information Technology
// 2022
//
#include <malloc.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "bit_array.h"

void bitArray_init(BitArray *arr, size_t len) {
    arr->bit_len = 0;
    arr->len = len;
    arr->data = (uint8_t*) malloc(len * sizeof(uint8_t));
}

void bitArray_destroy(BitArray *arr) {
    arr->bit_len = 0;
    arr->len = 0;
    free(arr->data);
}

bool bitArray_get(BitArray *arr, size_t bit_num) {
    assert(bit_num < arr->bit_len);
    size_t byte_num = bit_num / 8;
    size_t bit_in_byte = bit_num % 8;
    return GET_BIT(arr->data[byte_num], bit_in_byte);
}

void bitArray_set(BitArray *arr, size_t bit_num, bool bit) {
    assert(bit_num < arr->bit_len);
    size_t byte_num = bit_num / 8;
    size_t bit_in_byte = bit_num % 8;

    if(bit)
        SET_BIT(arr->data[byte_num], bit_in_byte);
    else
        CLEAR_BIT(arr->data[byte_num], bit_in_byte);
}

void bitArray_append(BitArray *arr, bool bit) {
    assert(arr->len * 8 > arr->bit_len); // still have space in the buffer
    arr->bit_len += 1;
    bitArray_set(arr, arr->bit_len - 1, bit);
}

void bitArray_remove(BitArray *arr) {
    assert(arr->bit_len > 0);
    arr->bit_len -= 1;
}

size_t bitArray_getBytes(BitArray *arr) {
    return arr->bit_len / 8;
}

void bitArray_dump(BitArray *arr) {
    for (int i = 0; i < bitArray_getBytes(arr); ++i) {
        printf("%d\t", i);
        for (int j = 0; j < 8; ++j) {
            printf("%d", bitArray_get(arr, i*8 + j));
        }
        printf(" (%02X)\n", arr->data[i]);
    }
}

size_t bitArray_copyData(BitArray *arr, uint8_t *buffer, size_t len) {
    size_t valid_bytes = bitArray_getBytes(arr);
    assert(len >= valid_bytes);
    memcpy(buffer, arr->data, valid_bytes);
    return valid_bytes;
}

bool array_getBit(const uint8_t* data, const size_t len, const size_t bit_num) {
    assert(len * 8 > bit_num);
    size_t byte_num = bit_num / 8;
    size_t bit_in_byte = bit_num % 8;
    return GET_BIT(data[byte_num], bit_in_byte);
}
