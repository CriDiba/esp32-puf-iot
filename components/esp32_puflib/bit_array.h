//
// Ondrej Stanicek
// staniond@fit.cvut.cz
// Czech Technical University - Faculty of Information Technology
// 2022
//
#ifndef TEST_BIT_ARRAY_H
#define TEST_BIT_ARRAY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define GET_BIT(val, bit)	(((val) >> (bit)) & 1)
#define SET_BIT(val, bit)	((val) |= (1 << (bit)))
#define CLEAR_BIT(val, bit)	((val) &= (~(1 << (bit))))
#define HIGHEST_BIT(val) (GET_BIT(val, 7))

/**
 * This struct represents an array of bits. It can be manipulated through functions defined in this header.
 */
typedef struct {
    uint8_t* data;   // data buffer
    size_t bit_len;  // number of valid bits in the data buffer
    size_t len;      // length of the data buffer in bytes
} BitArray;

/**
 * Initializes the BitArray struct to an empty bit array.
 * @param arr Pointer to the BitArray to initialize
 * @param len allocates enough space to store \p len*8 bits (\p len bytes)
 */
void bitArray_init(BitArray* arr, size_t len);

/**
 * Destroys the BitArray struct. Frees all allocated memory.
 * @param arr pointer to the BitArray struct to destroy
 */
void bitArray_destroy(BitArray* arr);

/**
 * Retrieves one bit from the BitArray
 * @param arr pointer to the BitArray
 * @param bit_num index of bit to return - bitArray_append must have been called at least
 * \p bit_num + 1 times for this to work (to have enough valid bits)
 * @return the bit which has the given index
 */
bool bitArray_get(BitArray* arr, size_t bit_num);

/**
 * Sets one bit from the BitArray
 * @param arr pointer to the BitArray
 * @param bit_num index of bit to return - bitArray_append must have been called at least
 * \p bit_num + 1 times for this to work (to have enough valid bits)
 * @param bit the new bit value to be set
 */
void bitArray_set(BitArray* arr, size_t bit_num, bool bit);

/**
 * Appends a bit to the end of the BitArray.
 * Needs to have enough space for the additional bit.
 * @param arr pointer to the BitArray
 * @param bit the new bit value to append
 */
void bitArray_append(BitArray* arr, bool bit);

/**
 * Removes the last bit from the bitArray.
 * The BitArray needs to have non-zero number of bits.
 * @param arr pointer to the BitArray
 */
void bitArray_remove(BitArray* arr);

/**
 * Returns the number of valid bytes - number of bytes that consist only of valid bits of the bitArray
 * @param arr pointer to the BitArray
 * @return number of valid bytes
 */
size_t bitArray_getBytes(BitArray* arr);

/**
 * Dumps the contents of the bitArray to stdout for debugging purposes
 * @param arr pointer to the BitArray
 */
void bitArray_dump(BitArray* arr);

/**
 * Copies all bits of the BitArray to the given \p buffer.
 * @param arr pointer to the BitArray
 * @param buffer the buffer to which write the data
 * @param len length of the \p buffer in bytes
 * @return number of valid bytes written to the buffer
 */
size_t bitArray_copyData(BitArray* arr, uint8_t* buffer, size_t len);

/**
 * Helper function that returns bit with index \p bit_num from the data arrray.
 * @param data the array from which the bits is retreived
 * @param len length of the \p data array in bytes
 * @param bit_num the index of the wanted bit
 * @return bit from the \p data array with index \p bit_num
 */
bool array_getBit(const uint8_t* data, size_t len, size_t bit_num);

#endif //TEST_BIT_ARRAY_H
