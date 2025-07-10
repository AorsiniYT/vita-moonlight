
# ENET static library
if(CMAKE_SYSTEM_NAME STREQUAL "Vita" OR PLATFORM_PSV)
    add_library(enet STATIC
        enet/callbacks.c
        enet/compress.c
        enet/host.c
        enet/list.c
        enet/packet.c
        enet/peer.c
        enet/protocol.c
        enet/unix.c
    )
else()
    add_library(enet STATIC
        enet/callbacks.c
        enet/compress.c
        enet/host.c
        enet/list.c
        enet/packet.c
        enet/peer.c
        enet/protocol.c
        enet/unix.c
    )
endif()
target_include_directories(enet PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} enet/include)


# moonlight-common-c static library
add_library(moonlight-common-c STATIC
    moonlight-common-c/src/AudioStream.c
    moonlight-common-c/src/ByteBuffer.c
    moonlight-common-c/src/Connection.c
    moonlight-common-c/src/ConnectionTester.c
    moonlight-common-c/src/ControlStream.c
    moonlight-common-c/src/FakeCallbacks.c
    moonlight-common-c/src/InputStream.c
    moonlight-common-c/src/LinkedBlockingQueue.c
    moonlight-common-c/src/Misc.c
    moonlight-common-c/src/Platform.c
    moonlight-common-c/src/PlatformCrypto.c
    moonlight-common-c/src/PlatformSockets.c
    moonlight-common-c/src/RecorderCallbacks.c
    moonlight-common-c/src/RtpAudioQueue.c
    moonlight-common-c/src/RtpVideoQueue.c
    moonlight-common-c/src/RtspConnection.c
    moonlight-common-c/src/RtspParser.c
    moonlight-common-c/src/SdpGenerator.c
    moonlight-common-c/src/SimpleStun.c
    moonlight-common-c/src/VideoDepacketizer.c
    moonlight-common-c/src/VideoStream.c
    moonlight-common-c/reedsolomon/rs.c
)

# Hereda los includes de enet para que <enet/enet.h> funcione en moonlight-common-c
target_include_directories(moonlight-common-c PUBLIC moonlight-common-c/src moonlight-common-c/reedsolomon)
target_link_libraries(moonlight-common-c PUBLIC enet)


# h264bitstream static library
add_library(h264bitstream STATIC
    h264bitstream/h264_nal.c
    h264bitstream/h264_sei.c
    h264bitstream/h264_stream.c
)
target_include_directories(h264bitstream PUBLIC h264bitstream)

# inih static library
add_library(inih STATIC
    inih/ini.c
)
target_include_directories(inih PUBLIC inih)

add_subdirectory(borealis/library)

# mdnsniff portable library (Vita y Windows)
add_library(mdnsniff STATIC
    mdnsniff/udp_sniffer_vita.c
    mdnsniff/udp_sniffer_win.c
    mdnsniff/common/debugScreen.c
)
target_include_directories(mdnsniff PUBLIC mdnsniff mdnsniff/common)

# Enlazar stubs de Vita solo si se compila para PSVita
if(CMAKE_SYSTEM_NAME STREQUAL "Vita" OR PLATFORM_PSV)
    target_link_libraries(mdnsniff PUBLIC SceSysmodule_stub SceNet_stub SceNetCtl_stub SceCtrl_stub SceDisplay_stub)
endif()
