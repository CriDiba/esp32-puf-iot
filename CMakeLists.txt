cmake_minimum_required(VERSION 3.5)

# include ESP-IDF
include(${CMAKE_CURRENT_SOURCE_DIR}/esp-idf/tools/cmake/project.cmake)

# project name
project(esp32-puf-iot)

target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "certs/device.crt" TEXT)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "certs/amazon_rootca.crt" TEXT)
