# esp32_puf_sec

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
