//
// Ondrej Stanicek
// staniond@fit.cvut.cz
// Czech Technical University - Faculty of Information Technology
// 2022
//
#include <stdio.h>
#include <string.h>
#include <soc/rtc.h>
#include <esp_sleep.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "puf_measurement.h"
#include "bit_array.h"
#include "nvs.h"
#include "ecc.h"

#define PUF_RESPONSE_SLEEP_uS (10*1000)
#define PUFSLEEP_RESPONSE_SLEEP_uS (100000)
#define PUF_HW_THRESHOLD_PERCENT (48.5)
#define PUF_ERROR_THRESHOLD_PERCENT (0.15)

uint8_t* RTC_FAST_MEMORY = (uint8_t*) RTC_FAST_MEMORY_ADDRESS;
uint8_t __NOINIT_ATTR PUF_BUFFER[PUF_MEMORY_SIZE];
PuflibState RTC_DATA_ATTR PUFLIB_STATE = {.state = NONE, .iteration_progress = 0};

uint8_t *PUF_RESPONSE = 0;
size_t PUF_RESPONSE_LEN = 0;
enum PufState RTC_DATA_ATTR PUF_STATE = RESPONSE_CLEAN;

void puf_response_reset_calculate();

void puflib_init() {
    switch(PUFLIB_STATE.state) {
        case NONE:
            return;
        case PROVISIONING:
            PUFLIB_STATE.iteration_progress += 1;
            provision_puf_calculate();
            break;
        case PUF_RESPONSE_RESET:
            puf_response_reset_calculate();
            break;
    }
}

void power_down_rtc_sram() {
    CLEAR_PERI_REG_MASK(RTC_CNTL_PWC_REG, RTC_CNTL_FASTMEM_FORCE_PU | RTC_CNTL_FASTMEM_FORCE_NOISO);
    SET_PERI_REG_MASK(RTC_CNTL_PWC_REG, RTC_CNTL_FASTMEM_FORCE_PD | RTC_CNTL_FASTMEM_FORCE_ISO);
}

void power_up_rtc_sram() {
    CLEAR_PERI_REG_MASK(RTC_CNTL_PWC_REG, RTC_CNTL_FASTMEM_FORCE_PD | RTC_CNTL_FASTMEM_FORCE_ISO);
    SET_PERI_REG_MASK(RTC_CNTL_PWC_REG, RTC_CNTL_FASTMEM_FORCE_PU | RTC_CNTL_FASTMEM_FORCE_NOISO);
}

void restore_rtc_sram(uint8_t *backup) {
    memcpy(RTC_FAST_MEMORY, backup, PUF_MEMORY_SIZE);
    free(backup);
}

uint8_t *backup_rtc_sram() {
    uint8_t *backup = malloc(PUF_MEMORY_SIZE);
    memcpy(backup, RTC_FAST_MEMORY, PUF_MEMORY_SIZE);
    return backup;
}

void turn_off_rtc_sram(int sleep_us) {
    power_down_rtc_sram();
    ets_delay_us(sleep_us); // busy loop
    power_up_rtc_sram();
    vTaskDelay(10 / portTICK_PERIOD_MS); // wait till sram really turns on and stabilizes (not necessary?)
}

void get_puf_bit_frequency(uint16_t *puf_freq, const size_t len, const size_t measurements) {
    assert(len == PUF_MEMORY_SIZE * 8); // puf_freq is a frequency of each bit --> needs to be 8 times larger
    uint8_t *backup = backup_rtc_sram();
    for (size_t i = 0; i < measurements; ++i) {
        turn_off_rtc_sram(PUF_RESPONSE_SLEEP_uS);
        for (int j = 0; j < len; ++j) {
            puf_freq[j] += array_getBit(RTC_FAST_MEMORY, PUF_MEMORY_SIZE, j);
        }
    }
    restore_rtc_sram(backup);
}

void pufsleep_bit_frequency_helper(const size_t len, bool first_iteration, bool last_iteration) {
    uint16_t *puf_freq;
    if (first_iteration) {
        puf_freq = calloc(sizeof(uint16_t), len);
        set_blob((const uint8_t *) puf_freq, len * sizeof(uint16_t), PUF_FREQUENCY_KEY);

    } else {
        size_t byte_len;
        get_blob((uint8_t **) &puf_freq, &byte_len, PUF_FREQUENCY_KEY);
        assert(byte_len == len * sizeof(uint16_t));
        for (int j = 0; j < len; ++j) {
            puf_freq[j] += array_getBit(PUF_BUFFER, PUF_MEMORY_SIZE, j);
        }
        set_blob((const uint8_t *) puf_freq, len * sizeof(uint16_t), PUF_FREQUENCY_KEY);
        free(puf_freq);
    }

    if(!last_iteration){
        esp_sleep_enable_timer_wakeup(PUFSLEEP_RESPONSE_SLEEP_uS);
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
        esp_deep_sleep_start();
    }
}

void get_pufsleep_bit_frequency(uint16_t **puf_freq, const size_t len, const size_t measurements, const int iteration_progress) {
    assert(len == PUF_MEMORY_SIZE * 8); // puf_freq is a frequency of each bit --> needs to be 8 times larger

    if(iteration_progress <= measurements) {
        pufsleep_bit_frequency_helper(len, iteration_progress == 0, iteration_progress == measurements);
    }

    size_t freq_len;
    get_blob((uint8_t **) puf_freq, &freq_len, PUF_FREQUENCY_KEY);
    // TODO overwrite and erase from NVS

    assert(freq_len == PUF_MEMORY_SIZE * 8 * sizeof(uint16_t)); // one uint16_t for each puf response byte
}


bool get_puf_response() {
    uint8_t * backup = backup_rtc_sram();

    // get stable bit mask
    uint8_t *mask;
    size_t mask_len;
    get_blob(&mask, &mask_len, PUF_MASK_KEY);
    assert(mask_len == PUF_MEMORY_SIZE);

    // get ecc data
    uint8_t *ecc_data;
    size_t ecc_len;
    get_blob(&ecc_data, &ecc_len, ECC_DATA_KEY);
    PUF_RESPONSE_LEN = ecc_len / 8; // 8x repetition code results in 8x smaller response

    // measure PUF response and apply the mask
    memset(RTC_FAST_MEMORY, 0x00, PUF_MEMORY_SIZE);
    turn_off_rtc_sram(PUF_RESPONSE_SLEEP_uS);

    double puf_hw_percent = (double) 100 * hamming_weight(RTC_FAST_MEMORY, PUF_MEMORY_SIZE) / (PUF_MEMORY_SIZE * 8);

    uint8_t *masked_puf = malloc(ecc_len);
    apply_puf_mask(mask, ecc_len*8, RTC_FAST_MEMORY, PUF_MEMORY_SIZE, masked_puf, ecc_len);

    // correct the masked response using the ECC data
    PUF_RESPONSE = malloc(PUF_RESPONSE_LEN);
    int bit_errors = correct_data(masked_puf, ecc_data, ecc_len, PUF_RESPONSE, PUF_RESPONSE_LEN);
    double puf_errors_percent = (double) 100 * bit_errors / (PUF_MEMORY_SIZE * 8);

    restore_rtc_sram(backup);
    free(ecc_data);
    free(mask);
    free(masked_puf);

    PUF_STATE = RESPONSE_READY;

    bool puf_ok = puf_hw_percent > PUF_HW_THRESHOLD_PERCENT && puf_errors_percent < PUF_ERROR_THRESHOLD_PERCENT;
    if(!puf_ok) {
        clean_puf_response();
    }
    return puf_ok;
}


_Noreturn void get_puf_response_reset() {
        PUFLIB_STATE.state = PUF_RESPONSE_RESET;
        esp_sleep_enable_timer_wakeup(PUFSLEEP_RESPONSE_SLEEP_uS);
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
        esp_deep_sleep_start();
}


void puf_response_reset_calculate() {
    assert(PUFLIB_STATE.state == PUF_RESPONSE_RESET);

    // get stable bit mask
    uint8_t *mask;
    size_t mask_len;
    get_blob(&mask, &mask_len, PUF_SLEEP_MASK_KEY);
    assert(mask_len == PUF_MEMORY_SIZE);

    // get ecc data
    uint8_t *ecc_data;
    size_t ecc_len;
    get_blob(&ecc_data, &ecc_len, ECC_SLEEP_DATA_KEY);
    PUF_RESPONSE_LEN = ecc_len / 8;

    // apply mask
    uint8_t *masked_puf = malloc(ecc_len);
    apply_puf_mask(mask, ecc_len * 8, PUF_BUFFER, PUF_MEMORY_SIZE, masked_puf, ecc_len);

    // correct the masked response using the ECC data
    PUF_RESPONSE = malloc(PUF_RESPONSE_LEN);
    correct_data(masked_puf, ecc_data, ecc_len, PUF_RESPONSE, PUF_RESPONSE_LEN);

    PUFLIB_STATE.state = NONE;
    PUF_STATE = RESPONSE_READY;

    free(ecc_data);
    free(mask);
    free(masked_puf);
}

void clean_puf_response() {
    memset(PUF_RESPONSE, 0x00, PUF_RESPONSE_LEN);
    free(PUF_RESPONSE);
    PUF_RESPONSE_LEN = 0;
    PUF_RESPONSE = NULL;
    PUF_STATE = RESPONSE_CLEAN;
}

