# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

if(CONFIG_DEVICE_SENSOR)
    list(FILTER app_sources EXCLUDE REGEX "pump.")
elseif(CONFIG_DEVICE_PUMP)
    list(FILTER app_sources EXCLUDE REGEX "sensor.")
endif()

idf_component_register(SRCS ${app_sources})

target_compile_options(${COMPONENT_LIB} PRIVATE SMARTGARDEN_BOARD)