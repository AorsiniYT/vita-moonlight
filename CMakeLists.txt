
cmake_minimum_required(VERSION 3.10)

set(BOREALIS_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/third_party/borealis/library)

# build options
include(${BOREALIS_LIBRARY}/cmake/commonOption.cmake)

# Dependencies
option(USE_SHARED_LIB "Whether to use shared libs provided by system" OFF)
cmake_dependent_option(USE_SYSTEM_FMT "" OFF "NOT USE_SHARED_LIB" ON)
cmake_dependent_option(USE_SYSTEM_TINYXML2 "" OFF "NOT USE_SHARED_LIB" ON)
cmake_dependent_option(USE_SYSTEM_TWEENY "" OFF "NOT USE_SHARED_LIB" ON)

## Soporte solo para Windows y PSVita

# toolchain
include(${BOREALIS_LIBRARY}/cmake/toolchain.cmake)

# project info
project(moonlight_vita)

## Forzar rutas de OpenSSL para MinGW/Windows (antes de cualquier add_subdirectory o find_package)
message(STATUS "CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
if(WIN32)
  set(OPENSSL_ROOT_DIR "${CMAKE_SOURCE_DIR}/openssl-mingw" CACHE PATH "" FORCE)
  set(OPENSSL_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/openssl-mingw/include" CACHE PATH "" FORCE)
  set(OPENSSL_CRYPTO_LIBRARY "${CMAKE_SOURCE_DIR}/openssl-mingw/lib/libcrypto.a" CACHE FILEPATH "" FORCE)
  set(OPENSSL_SSL_LIBRARY "${CMAKE_SOURCE_DIR}/openssl-mingw/lib/libssl.a" CACHE FILEPATH "" FORCE)
  set(OPENSSL_LIBRARIES "${OPENSSL_CRYPTO_LIBRARY};${OPENSSL_SSL_LIBRARY}" CACHE STRING "" FORCE)
endif()
set(TITLE "Moonlight")
set(VERSION_MAJOR "1")
set(VERSION_MINOR "0")
set(VERSION_ALTER "0")
set(VERSION_BUILD "0")
set(PSN_TITLE_ID  "MNLHTVITA")
set(PSN_VERSION  "01.00")
set(PROJECT_AUTHOR "AorsiniYT")
set(PROJECT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/img/demo_icon.jpg)
set(PROJECT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources)

if (USE_LIBROMFS)
    add_libromfs(${PROJECT_NAME} ${PROJECT_RESOURCES})
endif ()


add_subdirectory(third_party)

find_package(Threads REQUIRED)
list(APPEND APP_PLATFORM_LIB ${CMAKE_THREAD_LIBS_INIT})

# setting src and include

list(APPEND APP_PLATFORM_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/moonlight/include)
file(GLOB_RECURSE MAIN_SRC moonlight/*.cpp)



set(PLATFORM_OPTION)
if (WIN32)
    configure_file("${CMAKE_SOURCE_DIR}/moonlight/resource.rc.in" "${CMAKE_BINARY_DIR}/resource.rc")
    list(APPEND MAIN_SRC ${CMAKE_BINARY_DIR}/resource.rc)
    if (MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static" CACHE STRING "" FORCE)
    endif ()
elseif (PLATFORM_PSV)
    add_definitions(-DUSE_OPENSSL_CRYPTO)
    add_definitions(-D__vita__ -D__PSV__)
endif ()



# Configuración de curl y OpenSSL para MinGW/Windows
if(WIN32)
    include_directories(SYSTEM /usr/x86_64-w64-mingw32/include)
    link_directories(/usr/x86_64-w64-mingw32/lib)
    include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/openssl-mingw/include)
    link_directories(${CMAKE_SOURCE_DIR}/openssl-mingw/lib)
    add_definitions(-DOPENSSL_NO_SECURE_MEMORY)
endif()

# building target
program_target(${PROJECT_NAME} "${MAIN_SRC}")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)


# building release file
if (WIN32)
    add_custom_target(${PROJECT_NAME}.data
            COMMAND "${CMAKE_COMMAND}" -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_CURRENT_BINARY_DIR}/resources
    )
    if (NOT USE_LIBROMFS)
        add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}.data)
    endif ()
elseif (PLATFORM_PSV)
    set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d ATTRIBUTE2=12") # max heap size mode
    vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME} UNSAFE)
    set(PSV_ASSETS_FILES ${CMAKE_SOURCE_DIR}/psv/sce_sys sce_sys)
    if (NOT USE_LIBROMFS)
        list(APPEND PSV_ASSETS_FILES ${CMAKE_SOURCE_DIR}/resources resources)
    endif ()
    if (NOT USE_GXM)
        list(APPEND PSV_ASSETS_FILES ${CMAKE_SOURCE_DIR}/psv/module/ module)
    endif ()
    if (USE_VITA_SHARK)
        list(APPEND PSV_ASSETS_FILES "${CMAKE_BINARY_DIR}/vendor/SceShaccCg" module)
    endif ()
    vita_create_vpk(${PROJECT_NAME}.vpk ${PSN_TITLE_ID} ${PROJECT_NAME}.self
            VERSION ${PSN_VERSION}
            NAME ${TITLE}
            FILE ${PSV_ASSETS_FILES}

    )
endif ()


include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/inih)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/moonlight-common-c/reedsolomon)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/moonlight-common-c/src)

target_include_directories(${PROJECT_NAME} PRIVATE moonlight ${APP_PLATFORM_INCLUDE} ${CMAKE_SOURCE_DIR}/third_party ${CMAKE_SOURCE_DIR}/third_party/moonlight-common-c/reedsolomon ${CMAKE_SOURCE_DIR}/third_party/moonlight-common-c/src)

# Incluye los headers principales, crypto y libgamestream para todo el proyecto
target_include_directories(${PROJECT_NAME} PUBLIC 
    moonlight/include
    moonlight/src/crypto
    third_party/libgamestream
)
target_compile_options(${PROJECT_NAME} PRIVATE -ffunction-sections -fdata-sections ${APP_PLATFORM_OPTION})
target_link_libraries(${PROJECT_NAME} PRIVATE borealis ${APP_PLATFORM_LIB} inih mdnsniff libgamestream)




if(PLATFORM_PSV)
    # Enlaza todas las librerías de PSVita necesarias (igual que moonlight-legacy)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        -lSceVshBridge_stub
        -lexpat
        -lcurl
        -lmbedtls
        -lmbedx509
        -lmbedcrypto
        inih
        mdnsniff
        enet
        h264bitstream
        moonlight-common-c
        -lz
        -lssl
        -lcrypto
        -lopus
        -lbz2
        -lpthread
        -lSceDisplay_stub
        -lSceCtrl_stub
        -lSceTouch_stub
        -lSceNet_stub
        -lSceNetCtl_stub
        -lScePower_stub
        -lSceVideodec_stub
        -lSceCommonDialog_stub
        -lSceAppMgr_stub
        -lSceAudio_stub
        -lSceIme_stub
        -lSceMotion_stub
        -lvita2d
        -lSceGxm_stub
        -lScePgf_stub
        -lSceCommonDialog_stub
        -lfreetype
        -lpng
        -ljpeg
        -lz
        -lm
        -lc
    )
    include_directories(
    include/
    third_party/moonlight-common-c/src/
    third_party/moonlight-common-c/reedsolomon/
    third_party/libgamestream/
    third_party/h264bitstream/
    third_party/enet/include/
    third_party/inih/
    third_party/mdnsniff/
)
endif()


# Enlace de librerías para Windows (sin expat/XML, usando inih, emparejamiento requiere OpenSSL)
if(WIN32)
 # Asegura que el linker busque en la ruta de MinGW para expat y otras libs
    target_link_directories(${PROJECT_NAME} PRIVATE /usr/x86_64-w64-mingw32/lib)
    # 
    # El orden de las librerías es crítico en MinGW/Windows
    # Enlazamos contra los import libraries (.dll.a) de OpenSSL y curl
    target_link_libraries(${PROJECT_NAME} PRIVATE
        borealis
        ${APP_PLATFORM_LIB}
        inih
        mdnsniff
        enet
        h264bitstream
        moonlight-common-c
        z
        ws2_32
        crypt32
        iphlpapi
        winmm
        bcrypt
        rpcrt4
        user32
        gdi32
        advapi32
        shell32
        /usr/x86_64-w64-mingw32/lib/libexpat.dll.a
        ${CMAKE_SOURCE_DIR}/openssl-mingw/lib/libssl.a
        ${CMAKE_SOURCE_DIR}/openssl-mingw/lib/libcrypto.a
        ${CMAKE_SOURCE_DIR}/openssl-mingw/lib/libcurl.dll.a
    )
    # Copia las DLLs de OpenSSL y libcurl al directorio de salida tras compilar
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/openssl-mingw/bin/libssl-1_1-x64.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/openssl-mingw/bin/libcrypto-1_1-x64.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/openssl-mingw/bin/libcurl-4.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "/usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "/usr/x86_64-w64-mingw32/bin/libexpat-1.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/third_party/moonlight-common-c/libmoonlight-common-c.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endif()