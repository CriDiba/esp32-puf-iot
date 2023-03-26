//
// Ondrej Stanicek
// staniond@fit.cvut.cz
// Czech Technical University - Faculty of Information Technology
// 2022
//
#include <string.h>
#include <esp_rom_sys.h>

#include "wake_up_stub.h"
#include "puf_measurement.h"


void RTC_IRAM_ATTR puflib_wake_up_stub(void) {
    memcpy(PUF_BUFFER, (uint8_t*) DATA_SRAM_MEMORY_ADDRESS, PUF_MEMORY_SIZE);
}
