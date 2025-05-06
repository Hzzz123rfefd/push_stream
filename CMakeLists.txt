cmake_minimum_required(VERSION 3.15)
project(push_stream)
include(ExternalProject)

set(3PARTY_BUILD_DIR ${CMAKE_BINARY_DIR}/third_party)
## 导入x264
set(X264_SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/x264)
ExternalProject_Add(
    x264
    SOURCE_DIR ${X264_SOURCE_DIR}
    CONFIGURE_COMMAND ${X264_SOURCE_DIR}/configure --enable-shared --prefix=${3PARTY_BUILD_DIR}
    BUILD_COMMAND make
    INSTALL_COMMAND  make install
    BUILD_IN_SOURCE 0
    BUILD_ALWAYS 0
)
add_custom_command(
    TARGET x264 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/x264-prefix
)

# 导入lame环境
set(LAME_SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/lame)
ExternalProject_Add(
    lame
    SOURCE_DIR ${LAME_SOURCE_DIR}
    CONFIGURE_COMMAND ${LAME_SOURCE_DIR}/configure --prefix=${3PARTY_BUILD_DIR}
    BUILD_COMMAND make
    INSTALL_COMMAND  make install
    BUILD_IN_SOURCE 0
    BUILD_ALWAYS 0
)
add_custom_command(
    TARGET lame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/lame-prefix
)

## 导入SDL环境
set(SDL2_SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/SDL)
ExternalProject_Add(
    SDL
    SOURCE_DIR ${SDL2_SOURCE_DIR}
    CONFIGURE_COMMAND ${SDL2_SOURCE_DIR}/configure --prefix=${3PARTY_BUILD_DIR}
    BUILD_COMMAND make                                 
    INSTALL_COMMAND make install                          
    BUILD_IN_SOURCE 0
    BUILD_ALWAYS 0
)
add_custom_command(
    TARGET SDL POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/SDL-prefix
)


# 导入fdk环境
set(FDK_SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/fdk-aac)
# ExternalProject_Add(
#     fdk
#     SOURCE_DIR ${FDK_SOURCE_DIR}
#     CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
#     BUILD_COMMAND make                                 
#     INSTALL_COMMAND make install                          
#     BUILD_IN_SOURCE 1
# )
ExternalProject_Add(
    fdk
    SOURCE_DIR ${FDK_SOURCE_DIR}
    UPDATE_COMMAND ${FDK_SOURCE_DIR}/autogen.sh
    CONFIGURE_COMMAND ${FDK_SOURCE_DIR}/configure --prefix=${3PARTY_BUILD_DIR}
    BUILD_COMMAND make                                 
    INSTALL_COMMAND make install                          
    BUILD_IN_SOURCE 0
    BUILD_ALWAYS 0
)
add_custom_command(
    TARGET fdk POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/fdk-prefix
)

## 导入ffmpeg环境
set(FFMPEG_SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/ffmpeg)
ExternalProject_Add(
    ffmpeg
    SOURCE_DIR ${FFMPEG_SOURCE_DIR}
    CONFIGURE_COMMAND PKG_CONFIG_PATH=${3PARTY_BUILD_DIR}/lib/pkgconfig ${FFMPEG_SOURCE_DIR}/configure --enable-shared --enable-libx264 --enable-gpl --enable-libmp3lame --enable-nonfree --enable-sdl2 --enable-ffplay --enable-libfdk-aac --prefix=${3PARTY_BUILD_DIR}
    BUILD_COMMAND make
    INSTALL_COMMAND  make install
    BUILD_IN_SOURCE 0
    BUILD_ALWAYS 0
)

add_custom_command(
    TARGET ffmpeg POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/ffmpeg-prefix
)







