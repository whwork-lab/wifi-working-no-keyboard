# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 1.5.1)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico_w CACHE STRING "Board type" FORCE)
include(pico_sdk_import.cmake)

project(USB_DEVICE_AND_KEYBOARD C CXX ASM)
pico_sdk_init()

add_subdirectory(Pico-PIO-USB)
set(PICO_TINYUSB_SRC "${PICO_SDK_PATH}/lib/tinyusb/src")

add_executable(USB_DEVICE_AND_KEYBOARD 
    main_host.c 
    usb_descriptors.c 
    ${PICO_TINYUSB_SRC}/portable/raspberrypi/pio_usb/hcd_pio_usb.c
)

target_include_directories(USB_DEVICE_AND_KEYBOARD PRIVATE 
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/Pico-PIO-USB/src
    ${PICO_TINYUSB_SRC}/portable/raspberrypi/pio_usb
)

target_compile_definitions(USB_DEVICE_AND_KEYBOARD PRIVATE 
    PICO_BOARD="pico_w"
    CFG_TUSB_MCU=OPT_MCU_RP2040
    PICO_DEFAULT_LED_PIN_DISABLED=1 
)

target_link_libraries(USB_DEVICE_AND_KEYBOARD PUBLIC 
    pico_stdlib 
    pico_multicore 
    pico_pio_usb 
    tinyusb_device 
    tinyusb_host 
    pico_cyw43_arch_lwip_poll
)

pico_add_extra_outputs(USB_DEVICE_AND_KEYBOARD)
