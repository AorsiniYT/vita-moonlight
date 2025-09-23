# Video subsystem for Moonlight Vita
# Handles video decoding and rendering using PS Vita hardware

set(VIDEO_SOURCES
    vita.cpp
    vita.h
)

# Create video library
add_library(moonlight_video ${VIDEO_SOURCES})

# Include directories
target_include_directories(moonlight_video PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${VITA_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(moonlight_video PUBLIC
    moonlight_common
    ${VITA_LIBRARIES}
    SceVideodec_stub
    vita2d
    SceDisplay_stub
    SceGxm_stub
    SceSysmodule_stub
    SceKernel_stub
)

# Compile definitions
target_compile_definitions(moonlight_video PUBLIC
    VITA_VIDEO_LEGACY_MODE
)
