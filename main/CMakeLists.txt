file(GLOB_RECURSE SOURCES "./*.c")

set(INCLUDE_DIRS "./")

idf_component_register(SRCS ${SOURCES} INCLUDE_DIRS ${INCLUDE_DIRS})