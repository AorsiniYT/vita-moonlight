# FFmpeg Video subsystem CMakeLists.txt
# Sistema de video FFmpeg para Moonlight Vita

# Incluir directorios necesarios
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/libgamestream
    ${CMAKE_SOURCE_DIR}/moonlight/include
)

# Archivos fuente del sistema de video FFmpeg
set(FFMPEG_VIDEO_SOURCES
    ffmpeg.cpp
)

# Crear librería estática para el sistema de video FFmpeg
add_library(video_ffmpeg STATIC ${FFMPEG_VIDEO_SOURCES})

# TODO: Enlazar dependencias FFmpeg cuando se implemente
# target_link_libraries(video_ffmpeg
#     avcodec
#     avformat
#     avutil
#     swscale
#     vita2d
#     SceDisplay_stub
#     SceGxm_stub
# )

# Configurar propiedades de compilación
set_target_properties(video_ffmpeg PROPERTIES
    CXX_STANDARD 17
    C_STANDARD 11
)

# Incluir headers públicos
target_include_directories(video_ffmpeg PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
