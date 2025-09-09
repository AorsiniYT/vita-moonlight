cmake_minimum_required(VERSION 3.10)

# Definir la biblioteca estática libgamestream
add_library(libgamestream STATIC
    client.cpp
    http.cpp
    xml.cpp
)

# Incluir directorios necesarios
target_include_directories(libgamestream PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/moonlight-common-c/src
    ${CMAKE_SOURCE_DIR}/third_party/moonlight-common-c/reedsolomon
    ${CMAKE_SOURCE_DIR}/third_party/borealis/library/include
    ${CMAKE_SOURCE_DIR}/moonlight/include
)

# Enlazar dependencias
target_link_libraries(libgamestream PUBLIC
    borealis
    moonlight-common-c
    ${CURL_LIBRARIES}
    ${EXPAT_LIBRARIES}
    ${OPENSSL_LIBRARIES}
)

# Definiciones de compilación si es necesario
target_compile_definitions(libgamestream PRIVATE
    # Agregar defines específicos si los hay
)
