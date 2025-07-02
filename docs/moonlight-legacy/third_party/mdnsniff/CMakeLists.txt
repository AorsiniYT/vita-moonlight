cmake_minimum_required(VERSION 3.2)
project(test_vita_mdns C)

# --- Configuración General ---
get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
set(COMMON_SRC main.c common/debugScreen.c)
set(SNIFFER_SRC udp_sniffer_vita.c)

# --- Configuración multiplataforma ---
option(BUILD_MDNS_GUI "Build mDNS GUI (Win32)" ON)

# --- Legacy mDNS Vita (d3m3vilurr) ---
# add_subdirectory(${PARENT_DIR}/third_party/mdns_vita ${CMAKE_CURRENT_BINARY_DIR}/mdns_vita_build)

# Solo compilar ejecutable CLI Win si el compilador es MinGW o estamos en Windows
if(MINGW OR WIN32)
    message(STATUS "Configurando para Windows/MinGW (CLI)")
    add_executable(${PROJECT_NAME}_win udp_sniffer_win.c)
    target_include_directories(${PROJECT_NAME}_win PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/common"
    )
    target_link_libraries(${PROJECT_NAME}_win PRIVATE ws2_32 iphlpapi)
    target_compile_definitions(${PROJECT_NAME}_win PRIVATE UDP_SNIFFER_WIN_MAIN)
endif()

# --- Configuración para PS Vita (VitaSDK) ---
if(NOT WIN32 AND NOT MINGW)
    message(STATUS "Configurando para PS Vita (VitaSDK)")
    # Toolchain VitaSDK
    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
      if(DEFINED ENV{VITASDK})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
      else()
        message(FATAL_ERROR "Por favor define la variable de entorno VITASDK apuntando a tu VitaSDK.")
      endif()
    endif()

    include("${VITASDK}/share/vita.cmake" REQUIRED)
    link_directories("${VITASDK}/arm-vita-eabi/lib")

    add_compile_options(-g3 -Wall -Wextra -Wno-unused-parameter -Wno-pointer-sign -Wno-sign-compare -Wno-switch)
    set(CMAKE_C_FLAGS "-Wl,-q -g -std=c99 -D__vita__")

    set(TITLE_ID      "MDNS00001")
    set(TITLE         "Test Vita mDNS")
    set(VERSION_MAJOR "01")
    set(VERSION_MINOR "00")
    set(APP_VER "${VERSION_MAJOR}.${VERSION_MINOR}")

    add_executable(${PROJECT_NAME}.elf ${COMMON_SRC} ${SNIFFER_SRC})

    target_include_directories(${PROJECT_NAME}.elf PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/common"
    )

    target_link_libraries(${PROJECT_NAME}.elf
        SceSysmodule_stub
        SceNet_stub
        SceNetCtl_stub
        SceCtrl_stub
        SceDisplay_stub
    )

    # --- Creación de VPK ---
    add_custom_target(${PROJECT_NAME}.velf ALL
        COMMAND vita-elf-create ${PROJECT_NAME}.elf ${PROJECT_NAME}.velf
        DEPENDS ${PROJECT_NAME}.elf
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_target(param.sfo ALL
        COMMAND vita-mksfoex
                -s TITLE_ID=${TITLE_ID}
                -s APP_VER=${APP_VER}
                "${TITLE}"
                param.sfo
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_target(${PROJECT_NAME}.vpk ALL
        DEPENDS ${PROJECT_NAME}.velf param.sfo
        COMMAND vita-make-fself -s -c ${PROJECT_NAME}.velf eboot.bin
        COMMAND vita-pack-vpk
                -s param.sfo
                -b eboot.bin
                ${PROJECT_NAME}.vpk
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_dependencies(${PROJECT_NAME}.velf   ${PROJECT_NAME}.elf)
    add_dependencies(${PROJECT_NAME}.vpk    ${PROJECT_NAME}.velf param.sfo)
endif()
