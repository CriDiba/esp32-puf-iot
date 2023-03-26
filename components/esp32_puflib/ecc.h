//
// Ondrej Stanicek
// staniond@fit.cvut.cz
// Czech Technical University - Faculty of Information Technology
// 2022
//
#ifndef ESP32_PUF_ECC_H
#define ESP32_PUF_ECC_H

/**
 * Calculated the hamming weight (number of 1 bits) in a given buffer
 * @param byte the buffer from which the hamming weight is calculated
 * @param len the length of the \p buffer in bytes
 * @return the hamming weight
 */
int hamming_weight(const uint8_t *byte, size_t len);

/**
 * Provisions the PUF on this device - saves stable bit mask and ECC data to flash
 * for stable PUF response reconstruction
 */
void provision_puf_rtc();

/**
 * Provisions the PUF on this device - saves stable bit mask and ECC data to flash
 * for stable PUF response reconstruction
 * Uses the deep sleep PUF method.
 */
void provision_puf_sleep(int iteration_progress);

void provision_puf_calculate();

/**
 * Provisions the PUF on this device - saves stable bit mask and ECC data to flash
 * for stable PUF response reconstruction.
 * Provisions PUF for both RTC and deep sleep methods.
 */
void enroll_puf();

/**
 * Corrects the PUF response using the ECC data. The ECC used is 8x repetition code.
 * @param masked_data the PUF response data to be corrected
 * @param ecc_data the ECC helper data
 * @param len length of the \p masked_data and \p ecc_data in bytes (their length needs to be the same)
 * @param result array to which the corrected PUF response is saved
 * @param res_len length of the \p result array in bytes (needs to be len/8 because 8x repetition code is used)
 * @param the number of bits corrected
 */
int correct_data(const uint8_t* masked_data, const uint8_t* ecc_data, size_t len, uint8_t* result, size_t res_len);

/**
 * Applies the stable bit mask to the puf response - bits that have 0 bits in the mask are deleted.
 * @param mask mask of stable bits
 * @param mask_hw hamming distance of the mask - number of 1 bits (the mask itself can have more 1 bits,
 * but the calculation is stopped after \p mask_hw bits
 * @param puf_response puf response to apply the \p mask to
 * @param len length of the \p PUF_RESPONSE and \p mask in bytes (their length needs to be the same)
 * @param result array to which the masked puf response is saved
 * @param res_len length of the \p result array in bytes (needs to be \p mask_hw / 8)
 */
void apply_puf_mask(const uint8_t *mask, size_t mask_hw, uint8_t *puf_response,
                    size_t len, uint8_t *result, size_t res_len);

#endif //ESP32_PUF_ECC_H
