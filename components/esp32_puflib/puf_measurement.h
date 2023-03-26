//
// Ondrej Stanicek
// staniond@fit.cvut.cz
// Czech Technical University - Faculty of Information Technology
// 2022
//
#ifndef ESP32_PUF_PUF_MEASUREMENT_H
#define ESP32_PUF_PUF_MEASUREMENT_H

#include <esp_attr.h>

#define PUF_MEMORY_SIZE 0x1000 // max is 0x2000 - 8KB for RTC FAST SRAM
#define RTC_FAST_MEMORY_ADDRESS (0x3FF80000)
#define DATA_SRAM_MEMORY_ADDRESS (0x3FFB0000)

enum STATE {NONE = 0, PROVISIONING, PUF_RESPONSE_RESET};

typedef struct {
    enum STATE state;
    int iteration_progress;
} PuflibState;

enum PufState {RESPONSE_CLEAN, RESPONSE_READY};

extern uint8_t PUF_BUFFER[PUF_MEMORY_SIZE];
extern PuflibState PUFLIB_STATE;
extern uint8_t *PUF_RESPONSE;
extern size_t PUF_RESPONSE_LEN;
extern enum PufState PUF_STATE;

/**
 * This function needs to be called as the first function in main.
 * Initializes the library and creates a PUF response if PUF deepsleep wake up happened.
 */
void puflib_init();

/**
 * Powers down the RTC fast memory 8kB SRAM
 */
void power_down_rtc_sram();

/**
 * Powers up the RTC fast memory 8kB SRAM
 */
void power_up_rtc_sram();

/**
 * Backs up the contents of RTC fast memory to a buffer.
 * @return the buffer with the RTC fast memory backup
 */
uint8_t* backup_rtc_sram();

/**
 * Restores the RTC fast memory backup.
 * @param backup pass a pointer that was returned by the backup_rtc_sram function
 */
void restore_rtc_sram(uint8_t* backup);

/**
 * Powers the RTC fast memory down for \p sleep_us microseconds and then powers it up again.
 * @param sleep_us number of microseconds to leave the SRAM off
 */
void turn_off_rtc_sram(int sleep_us);

/**
 * Generates frequency array of the PUF response.
 * puf_freq[i] is the number of times the i-th bit in the puf response was 1 during \p measurements measurements.
 * (if the bit is 1 all the time, puf_freq[i] == \p measurements)
 * @param puf_freq the resulting frequency array
 * @param len length of the \p puf_freq (needs to be 8 * size of the PUF SRAM region read)
 * @param measurements number of PUF measurements to take
 */
void get_puf_bit_frequency(uint16_t *puf_freq, size_t len, size_t measurements);

/**
 * Generates frequency array of the PUF response.
 * puf_freq[i] is the number of times the i-th bit in the puf response was 1 during \p measurements measurements.
 * (if the bit is 1 all the time, puf_freq[i] == \p measurements)
 * Deep sleep PUF version is used.
 * @param puf_freq the resulting frequency array will be set to this buffer
 * @param len length of the \p puf_freq (needs to be 8 * size of the PUF SRAM region read)
 * @param measurements number of PUF measurements to take
 * @param iteration_progress which iteration should be now executed
 */
void get_pufsleep_bit_frequency(uint16_t **puf_freq, size_t len, size_t measurements, int iteration_progress);

_Bool get_puf_response();

void get_puf_response_reset();

void clean_puf_response();

#endif //ESP32_PUF_PUF_MEASUREMENT_H
