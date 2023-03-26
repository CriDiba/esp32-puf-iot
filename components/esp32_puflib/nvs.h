//
// Ondrej Stanicek
// staniond@fit.cvut.cz
// Czech Technical University - Faculty of Information Technology
// 2022
//
#ifndef ESP32_PUF_NVS_H
#define ESP32_PUF_NVS_H

#define ECC_DATA_KEY "ECC_DATA"
#define PUF_MASK_KEY "PUF_MASK"
#define ECC_SLEEP_DATA_KEY "ECC_SLEEP_DATA"
#define PUF_SLEEP_MASK_KEY "PUF_SLEEP_MASK"
#define PUF_FREQUENCY_KEY "PUF_FREQUENCY"

/**
 * Sets the blob data. This function does not recover from NVS errors and will crash the app on such errors.
 * @param blob buffer with the data to be saved to NVS
 * @param length length of the buffer in bytes
 */
void set_blob(const uint8_t *blob, size_t length, const char* key);


/**
 * Gets the blob data from NVS. This function returns false only when the blob data cannot be retrieved
 * (were not set in the first place). Fatal NVS errors (initialization errors etc.) will crash the app.
 * @param blob data buffer that will be set to the saved data, !NEEDS TO BE FREED!
 * @param length pointer to length that will be set to the size of the retrieved blob
 * @return true if the data was set correctly, false otherwise
 */
bool get_blob(uint8_t **blob, size_t *length, const char *key);

#endif //ESP32_PUF_NVS_H
