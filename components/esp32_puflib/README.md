# esp32_puflib
Library which implements a SRAM based physical unclonable function on ESP32 microntroller family.

## How to include a project:
The library is a ESP-IDF component ([ESP-IDF documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#example-project)).

To include this into your project, you have 2 options:
1. Clone this repo into your own project and the component is included automatically
2. Clone this repo into its own directory and add the path to the component search path variable:

add `set(EXTRA_COMPONENT_DIRS /path/to/the/parent/directory/of/this/component)` line to your project CMakeLists.txt file.

#### Partition table: (IMPORTANT)
the library saves the helper PUF data and data during enrollment to
the ESP-IDF NVS library. The default NVS partition is too small to save
all the needed data, so bigger partition is needed.

You can use this example partition table:
```
# ESP-IDF Partition Table
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x50000,
phy_init, data, phy,           ,  0x1000,
factory,  app,  factory,       ,  1M,
```
save it to a .csv file and add the path to the file in menuconfig (Partition table -> Custom partition CSV file)

## Minimal working example:
```c
    #include <stdio.h>
    #include <esp_sleep.h>

    #include <puflib.h>

    void app_main(void)
    {
        puflib_init(); // needs to be called first in app_main
        
        enroll_puf(); // enrollment needs to be done only once at the beginning

        // condition will be true, if a PUF response is ready (useful after a restart)
        if(PUF_STATE != RESPONSE_READY) {
            bool puf_ok = get_puf_response();
            if(!puf_ok) {
                get_puf_response_reset(); // the device resets now and the app starts again from app_main
            }
        }

        // PUF_RESPONSE_LEN is a PUF response length in bytes
        for (size_t i = 0; i < PUF_RESPONSE_LEN; ++i) {
            printf("%02X ", PUF_RESPONSE[i]); // PUF_RESPONSE is a buffer with the PUF response
        }
        printf("\n");

        clean_puf_response();
    }

    void RTC_IRAM_ATTR esp_wake_deep_sleep(void) {
        esp_default_wake_deep_sleep();
        puflib_wake_up_stub(); // needs to be called somewhere in wake up stub
    }
```

## Documentation
The API is documented in the puflib.h file that you need to include.