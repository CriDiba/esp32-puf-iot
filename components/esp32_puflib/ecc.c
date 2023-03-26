//
// Ondrej Stanicek
// staniond@fit.cvut.cz
// Czech Technical University - Faculty of Information Technology
// 2022
//
#include <stdio.h>
#include <math.h>
#include <esp_system.h>
#include "ecc.h"
#include "bit_array.h"
#include "nvs.h"
#include "puf_measurement.h"

#define PROVISIONING_MEASUREMENTS 10

// value of probability of bit flip to declare it stable (or lower)
// 0.001 works perfectly and gives about 380 bytes of PUF response
// 0.005 seems to work also and gives about 400 bytes of PUF response
#define STABLE_BIT_PROBABILITY 0.001

#define MASK_LOWER_BOUND ((int) round(PROVISIONING_MEASUREMENTS * STABLE_BIT_PROBABILITY))
#define MASK_UPPER_BOUND ((int) round(PROVISIONING_MEASUREMENTS * (1 - STABLE_BIT_PROBABILITY)))
#define MIN(a, b) (((a) < (b))? (a) : (b))

/**
 * Array of precalculated outputs of the majority_bit function for all of the possible bytes
 */
uint8_t majority_table[256] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1,
        0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1,
        0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1,
        0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1,
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

uint8_t hw_table[256] = {
        0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
};

int hamming_weight(const uint8_t *byte, const size_t len) {
    int counter = 0;
    for (size_t i = 0; i < len; ++i) {
        counter += hw_table[byte[i]];
    }
    return counter;
}

/**
 * Generates the ECC data for 8x repetition code from the PUF reference response.
 * @param puf_reference PUF reference response from which the ECC data is calculated
 * @param ecc_data resulting ECC data will be saved to this array
 * @param len length of both the \p puf_reference and \p ecc_data in bytes (their length needs to be the same)
 */
void generate_ecc_data(const uint8_t* puf_reference, uint8_t* ecc_data, const size_t len) {
    for (int i = 0; i < len; ++i) {
        if(HIGHEST_BIT(puf_reference[i])){
            ecc_data[i] = ~(puf_reference[i]);
        }else{
            ecc_data[i] = puf_reference[i];
        }
    }
}

/**
 * Generates the ECC data for 8x repetition code from the PUF reference response.
 * The ECC data will be generated according to the template in such a way, that the resulting PUF response will be the
 * same for \p puf_reference and for \p template_reference after using ECC on \p puf_reference.
 * @param puf_reference PUF reference response from which the ECC data is calculated
 * @param template_reference another PUF reference that will be used to calculate the ECC data
 * @param ecc_data resulting ECC data will be saved to this array
 * @param len length of the \p puf_reference, \p template_reference and \p ecc_data in bytes (their length needs to be the same)
 */
void generate_ecc_data_template(const uint8_t* puf_reference, const uint8_t *template_reference,
                                uint8_t* ecc_data, const size_t len) {
    for (int i = 0; i < len; ++i) {
        if(HIGHEST_BIT(template_reference[i])){
            ecc_data[i] = ~(puf_reference[i]);
        }else{
            ecc_data[i] = puf_reference[i];
        }
    }
}

int correct_data(const uint8_t* masked_data, const uint8_t* ecc_data, const size_t len,
                  uint8_t* result, const size_t res_len) {
    assert(res_len == len/8);
    BitArray arr;
    bitArray_init(&arr, res_len);

    int errors = 0; //debug
    for (int i = 0; i < len; ++i) {
        uint8_t code_word = masked_data[i] ^ ecc_data[i];
        bitArray_append(&arr, majority_table[code_word]);

        if(majority_table[code_word]) {
            code_word = ~code_word;
        }
        errors += hw_table[code_word];
    }

    bitArray_copyData(&arr, result, res_len);
    bitArray_destroy(&arr);
    return errors;
}

/**
 * Creates a mask of stable PUF bits from a frequency array.
 * @param puf_freq a frequency array of the PUF response
 * @param freq_len length of the \p puf_freq
 * @param mask the resulting mask will be saved to this array
 * @param mask_len length of the \p mask array in bytes (needs to be \p freq_len / 8)
 * @param mask_hw out param to which the mask hamming weight will be saved (the number of 1 bits of the mask)
 */
void create_puf_mask(const uint16_t *puf_freq, const size_t freq_len, uint8_t *mask, size_t mask_len, size_t* mask_hw) {
    assert(freq_len == mask_len * 8);
    BitArray arr;
    bitArray_init(&arr, mask_len);

    *mask_hw = 0;
    for (int i = 0; i < freq_len; ++i) {
        bool bit = puf_freq[i] >= MASK_UPPER_BOUND || puf_freq[i] <= MASK_LOWER_BOUND;
        *mask_hw += bit;
        bitArray_append(&arr, bit);
    }

    // round to the nearest lower multiple of 64
    // this means the resulting PUF response bits will be multiple of 8 - whole bytes (for convenience)
    *mask_hw -= *mask_hw % 64;
    bitArray_copyData(&arr, mask, mask_len);
    bitArray_destroy(&arr);
}

void apply_puf_mask(const uint8_t *mask, const size_t mask_hw, uint8_t *puf_response,
                    const size_t len, uint8_t *result, const size_t res_len) {
    assert(res_len == mask_hw/8);
    BitArray arr;
    bitArray_init(&arr, res_len);

    size_t bit_counter = 0;
    for (int i = 0; i < len * 8; ++i) {
        if(array_getBit(mask, len, i)) {
            bitArray_append(&arr, array_getBit(puf_response, len, i));
            bit_counter += 1;
            if(bit_counter >= mask_hw)
                break; // already added enough bits
        }
    }
    bitArray_copyData(&arr, result, res_len);
    bitArray_destroy(&arr);
}

/**
 * Creates a PUF reference response from the frequency array.
 * @param puf_freq a frequency array of the PUF response
 * @param freq_len length of the \p puf_freq
 * @param puf_reference an array to which the resulting PUF reference is saved
 * @param ref_len length of the \p puf_reference in bytes (needs to be freq_len / 8)
 */
void create_puf_reference(const uint16_t *puf_freq, const size_t freq_len,
                          uint8_t *puf_reference, const size_t ref_len) {
    assert(freq_len == ref_len * 8);
    BitArray arr;
    bitArray_init(&arr, ref_len);

    for (int i = 0; i < freq_len; ++i) {
        // bit is 0 in the reference iff it is 0 in more than half of the PUF measurements
        bool bit = puf_freq[i] > PROVISIONING_MEASUREMENTS/2;
        bitArray_append(&arr, bit);
    }

    bitArray_copyData(&arr, puf_reference, ref_len);
    bitArray_destroy(&arr);
}

void provision_puf_helper(uint16_t* puf_freq_rtc, uint16_t* puf_freq_sleep) {
    size_t puf_len = PUF_MEMORY_SIZE;

    // ----- generate stable bit masks from bit frequencies-----
    uint8_t *mask_rtc = malloc(puf_len);
    size_t mask_rtc_hw;
    create_puf_mask(puf_freq_rtc, puf_len * 8, mask_rtc, puf_len, &mask_rtc_hw);

    uint8_t *mask_sleep = malloc(puf_len);
    size_t mask_sleep_hw;
    create_puf_mask(puf_freq_sleep, puf_len * 8, mask_sleep, puf_len, &mask_sleep_hw);

    size_t mask_hw = MIN(mask_rtc_hw, mask_sleep_hw);
    printf("PUF bytes: %d\n", mask_hw/64);

    set_blob(mask_rtc, puf_len, PUF_MASK_KEY);
    set_blob(mask_sleep, puf_len, PUF_SLEEP_MASK_KEY);

    // ------ generate PUF references ------------
    uint8_t *puf_reference_rtc = malloc(puf_len);
    create_puf_reference(puf_freq_rtc, puf_len * 8, puf_reference_rtc, puf_len);
    uint8_t *puf_reference_sleep = malloc(puf_len);
    create_puf_reference(puf_freq_sleep, puf_len * 8, puf_reference_sleep, puf_len);

    // mask the reference to obtain only stable bits
    uint8_t *masked_reference_rtc = malloc(mask_hw / 8);
    apply_puf_mask(mask_rtc, mask_hw, puf_reference_rtc, puf_len, masked_reference_rtc, mask_hw / 8);
    uint8_t *masked_reference_sleep = malloc(mask_hw / 8);
    apply_puf_mask(mask_sleep, mask_hw, puf_reference_sleep, puf_len, masked_reference_sleep, mask_hw / 8);


    // generate ECC data
    uint8_t *ecc_data_sleep = malloc(mask_hw/8);
    generate_ecc_data(masked_reference_sleep, ecc_data_sleep, mask_hw / 8);
    set_blob(ecc_data_sleep, mask_hw/8, ECC_SLEEP_DATA_KEY);

    uint8_t *ecc_data_rtc = malloc(mask_hw/8);
    generate_ecc_data_template(masked_reference_rtc, masked_reference_sleep, ecc_data_rtc, mask_hw / 8);
    set_blob(ecc_data_rtc, mask_hw/8, ECC_DATA_KEY);

    free(mask_rtc);
    free(mask_sleep);
    free(puf_reference_rtc);
    free(puf_reference_sleep);
    free(masked_reference_rtc);
    free(masked_reference_sleep);
    free(ecc_data_sleep);
    free(ecc_data_rtc);
}


void provision_puf_calculate() {
    size_t puf_len = PUF_MEMORY_SIZE;

    // TODO optimize memory usage - dont load both puf_freq at once
    uint16_t *puf_freq_sleep;
    get_pufsleep_bit_frequency(&puf_freq_sleep, puf_len * 8,
                               PROVISIONING_MEASUREMENTS, PUFLIB_STATE.iteration_progress);

    uint16_t *puf_freq_rtc = calloc(sizeof(uint16_t), puf_len * 8);
    get_puf_bit_frequency(puf_freq_rtc, puf_len * 8, PROVISIONING_MEASUREMENTS);

    provision_puf_helper(puf_freq_rtc, puf_freq_sleep);

    free(puf_freq_rtc);
    free(puf_freq_sleep);
}

void enroll_puf() {
    if(PUFLIB_STATE.state == PROVISIONING) {
        PUFLIB_STATE.state = NONE;
        PUFLIB_STATE.iteration_progress = 0;
        printf("PROVISIONING DONE\n");
        return;
    }else if(PUFLIB_STATE.state == NONE) {
        printf("STARTING PROVISIONING\n");
        PUFLIB_STATE.state = PROVISIONING;
        PUFLIB_STATE.iteration_progress = 0;
        provision_puf_calculate();
    }
}
