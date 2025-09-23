# Legacy Video subsystem CMakeLists.txt
# Sistema de video legacy para Moonlight Vita (SceVideodec)

# Incluir directorios necesarios
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/libgamestream
    ${CMAKE_SOURCE_DIR}/moonlight/include
)

# Archivos fuente del sistema de video legacy
set(LEGACY_VIDEO_SOURCES
    vita.cpp
)

# Crear librería estática para el sistema de video legacy
add_library(video_legacy STATIC ${LEGACY_VIDEO_SOURCES})

# Enlazar dependencias necesarias
target_link_libraries(video_legacy
    vita2d
    SceVideodec_stub
    SceDisplay_stub
    SceGxm_stub
)

# Configurar propiedades de compilación
set_target_properties(video_legacy PROPERTIES
    CXX_STANDARD 17
    C_STANDARD 11
)

# Incluir headers públicos
target_include_directories(video_legacy PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
