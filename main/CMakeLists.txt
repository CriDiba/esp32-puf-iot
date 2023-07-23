file(GLOB_RECURSE SOURCES "src/*.c")

set(INCLUDE_DIRS "src/")

idf_component_register(SRCS ${SOURCES} INCLUDE_DIRS ${INCLUDE_DIRS})

# add coreMQTT library
include(lib/coreMQTT/mqttFilePaths.cmake)
add_library(coreMQTT ${MQTT_SOURCES} ${MQTT_SERIALIZER_SOURCES})
target_include_directories(coreMQTT PUBLIC ${MQTT_INCLUDE_PUBLIC_DIRS})
target_include_directories(coreMQTT PRIVATE ${IDF_INCLUDE_DIRECTORIES} ${INCLUDE_DIRS})
target_compile_options(coreMQTT PRIVATE ${IDF_COMPILE_OPTIONS})
target_compile_definitions(coreMQTT PRIVATE ${IDF_COMPILE_DEFINITIONS})
target_compile_definitions(coreMQTT PUBLIC MQTT_DO_NOT_USE_CUSTOM_CONFIG)

# add cJSON library
include(lib/cJSON/cJSON.cmake)
add_library(cJSON ${CJSON_SOURCES})
target_include_directories(cJSON PUBLIC ${CJSON_INCLUDE_PUBLIC_DIRS})
target_include_directories(cJSON PRIVATE ${IDF_INCLUDE_DIRECTORIES} ${INCLUDE_DIRS})
target_compile_options(cJSON PRIVATE ${IDF_COMPILE_OPTIONS})
target_compile_definitions(cJSON PRIVATE ${IDF_COMPILE_DEFINITIONS})

# link libraries
target_link_libraries(${COMPONENT_TARGET} coreMQTT)
target_link_libraries(${COMPONENT_TARGET} cJSON)