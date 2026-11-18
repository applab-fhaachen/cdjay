cmake_minimum_required(VERSION 3.13)

# Initialize Raspberry Pi Pico SDK
include(pico_sdk_import.cmake OPTIONAL)

project(cdjay_midi C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Check if PICO_SDK_PATH is set
if (NOT TARGET pico_stdlib)
    message(FATAL_ERROR "PICO_SDK_PATH is not defined. Please set it or build within the Raspberry Pi Pico VS Code environment.")
endif()

pico_sdk_init()

add_executable(cdjay_midi
    pico_c_sdk/main.c
    pico_c_sdk/usb_descriptors.c
)

# Include TinyUSB header path
target_include_directories(cdjay_midi PRIVATE
    pico_c_sdk
)

# Link hardware libraries and TinyUSB
target_link_libraries(cdjay_midi
    pico_stdlib
    hardware_gpio
    hardware_adc
    tinyusb_device
    tinyusb_board
)

# Enable USB output, disable UART
pico_enable_stdio_usb(cdjay_midi 0)
pico_enable_stdio_uart(cdjay_midi 0)

# Create map/bin/hex/uf2 file
pico_add_extra_outputs(cdjay_midi)
