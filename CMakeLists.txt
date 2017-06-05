cmake_minimum_required(VERSION 3.0)

project("ROjal_MQTT_Client" VERSION 0.0.1 LANGUAGES C)

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

include(CTest)

SET(CMAGE_CFLAGS "-g -O3 -Wall -W -NODEBUG")
SET(CMAGE_CXXFLAGS "-g -O3 -Wall -W -NODEBUG")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")

add_subdirectory(src)
add_subdirectory(test)
