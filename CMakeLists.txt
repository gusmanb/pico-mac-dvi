# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

#config options
set(SD_TX 19 CACHE STRING "SD SPI TX pin")
set(SD_RX 20 CACHE STRING "SD SPI RX pin")
set(SD_SCK 18 CACHE STRING "SD SPI SCK pin")
set(SD_CS 21 CACHE STRING "SD SPI CS pin")
set(SD_MHZ 50 CACHE STRING "SD SPI speed in MHz")
set(DVI_DEFAULT_SERIAL_CONFIG waveshare_rp2040_pizero)
set(MOUSE_DIVIDER 4.0f)

add_compile_definitions(SD_TX=${SD_TX} SD_RX=${SD_RX} SD_SCK=${SD_SCK} SD_CS=${SD_CS} SD_MHZ=${SD_MHZ} DVI_DEFAULT_SERIAL_CONFIG=${DVI_DEFAULT_SERIAL_CONFIG} MOUSE_DIVIDER=${MOUSE_DIVIDER})

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NEVER EDIT THE NEXT LINES for Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
    set(IS_WINDOWS 1)
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.0.0)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

# See below, -DMEMSIZE=<size in KB> will configure umac's memory size,
# overriding defaults.

project(firmware)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# For TUSB host stuff:
set(FAMILY rp2040)
set(BOARD raspberry_pi_pico)

set(TINYUSB_PATH ${PICO_SDK_PATH}/lib/tinyusb)

# umac subproject (and Musashi sub-subproject)
set(UMAC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/umac)
set(UMAC_MUSASHI_PATH ${UMAC_PATH}/external/Musashi)
set(UMAC_INCLUDE_PATHS ${UMAC_PATH}/include ${UMAC_MUSASHI_PATH})

# This isn't very nice, but hey it's Sunday :p
set(UMAC_SOURCES
  ${UMAC_PATH}/src/disc.c
  ${UMAC_PATH}/src/main.c
  ${UMAC_PATH}/src/rom.c
  ${UMAC_PATH}/src/scc.c
  ${UMAC_PATH}/src/via.c
  ${UMAC_MUSASHI_PATH}/m68kcpu.c
  ${UMAC_MUSASHI_PATH}/m68kdasm.c
  ${UMAC_MUSASHI_PATH}/m68kops.c
  ${UMAC_MUSASHI_PATH}/softfloat/softfloat.c
  )

set(MEMSIZE 208 CACHE STRING "Memory size, in KB")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -DPICO -DMUSASHI_CNF=\\\"../include/m68kconf.h\\\" -DUMAC_MEMSIZE=${MEMSIZE}")

add_compile_definitions(USE_SD=1)
add_compile_definitions(SD_TX=${SD_TX} SD_RX=${SD_RX} SD_SCK=${SD_SCK} SD_CS=${SD_CS} SD_MHZ=${SD_MHZ})
add_subdirectory(external/picofat)

# libdvi subproject
set(LIBDVI_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/picodvi/software/libdvi)
set(LIBDVI_INCLUDE_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/external/picodvi/software/include)
add_subdirectory(${LIBDVI_PATH})

if (TARGET tinyusb_device)
  add_executable(firmware
    src/main.c
    src/kbd.c
    src/hid.c
    src/video.c
    src/menu.c
    ${UMAC_SOURCES}
    )

  if(IS_WINDOWS)
    message (WARNING "******umac needs to be built manually under Windows!******")
  else()
    # The umac sources need to prepare Musashi (some sources are generated)
    add_custom_command(OUTPUT ${UMAC_MUSASHI_PATH}/m68kops.c
    COMMAND echo "*** Preparing umac source ***"
    COMMAND make -C ${UMAC_PATH} prepare
    )
  endif()

  # The umac sources need to prepare Musashi (some sources are generated):
  #add_custom_command(OUTPUT ${UMAC_MUSASHI_PATH}/m68kops.c
  #  COMMAND echo "*** Preparing umac source ***"
  #  COMMAND make -C ${UMAC_PATH} prepare
  #  )
  add_custom_target(prepare_umac
    DEPENDS ${UMAC_MUSASHI_PATH}/m68kops.c
    )
  add_dependencies(firmware prepare_umac)

  target_compile_definitions(firmware PRIVATE
	DVI_VERTICAL_REPEAT=1
	DVI_N_TMDS_BUFFERS=3
	DVI_MONOCHROME_TMDS
)


  target_link_libraries(firmware
    pico_stdlib
    pico_multicore
    tinyusb_host
    tinyusb_board
    hardware_dma
    hardware_pio
    hardware_sync
    libdvi
    pico_fatfs
    )

  target_include_directories(firmware PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${TINYUSB_PATH}/hw
    ${TINYUSB_PATH}/src
    ${UMAC_INCLUDE_PATHS}
    ${LIBDVI_PATH}
    ${LIBDVI_INCLUDE_PATHS}
    incbin
    )

  pico_enable_stdio_uart(firmware 1)

  # Needed for UF2:
  pico_add_extra_outputs(firmware)

elseif(PICO_ON_DEVICE)
   message(WARNING "not building firmware because TinyUSB submodule is not initialized in the SDK")
endif()

