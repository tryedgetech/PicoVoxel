cmake_minimum_required(VERSION 3.13)

set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

if (NOT PICO_BOARD)
	set(PICO_BOARD pico2 CACHE STRING "Pico board type")
endif()

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(PicoVoxel C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

set(PICOVOXEL_FEATURE_PRESET "default" CACHE STRING "PicoVoxel feature preset")
set_property(CACHE PICOVOXEL_FEATURE_PRESET PROPERTY STRINGS
default minimal joystick no_texture profile no_macrocell)
set(PICOVOXEL_FEATURE_CONFIG "config/features_${PICOVOXEL_FEATURE_PRESET}.h")

add_executable(PicoVoxel
	src/block.c
	src/input.c
	src/main.c
	src/player.c
	src/camera_automode.c
	src/renderer.c
	src/st7789.c
	src/world.c
	src/sound.c
	src/rle_comp.c
	assets/fonts/sk_font10.c
	assets/fonts/sk_font12.c
	assets/fonts/sk_font16.c
	assets/model/model_data.s
	external/hid_app.c
)

target_include_directories(PicoVoxel PRIVATE
	${CMAKE_CURRENT_LIST_DIR}/src
	${CMAKE_CURRENT_LIST_DIR}/assets/fonts
	${CMAKE_CURRENT_LIST_DIR}/assets/audio
	${CMAKE_CURRENT_LIST_DIR}/assets/model
	${CMAKE_CURRENT_LIST_DIR}/external
)

target_compile_options(PicoVoxel PRIVATE
	-Wall
	-Wextra
	-O3
#	-flto
)

target_compile_definitions(PicoVoxel PRIVATE
	PICOVOXEL_FEATURE_CONFIG="${PICOVOXEL_FEATURE_CONFIG}"
)

target_link_options(PicoVoxel PRIVATE
	-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/PicoVoxel.map
)

target_link_libraries(PicoVoxel PRIVATE
	pico_stdlib
	pico_multicore
	hardware_adc
	hardware_spi
	hardware_dma
	hardware_gpio
	hardware_pwm
	hardware_interp
	pico_rand

	tinyusb_board
	tinyusb_host
)

pico_enable_stdio_usb(PicoVoxel 0)
pico_enable_stdio_uart(PicoVoxel 1)

pico_add_extra_outputs(PicoVoxel)
