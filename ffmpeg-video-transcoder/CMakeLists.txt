cmake_minimum_required(VERSION 2.8)

project(ffmpeg-video-transcoder)

set(TARGET trans_decoder)
set(CMAKE_BUILD_TYPE RELEASE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 -fPIC -std=c++11")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O3 -fPIC")

include_directories(third/ffmpeg/include)
include_directories(third/opencv/include)
link_directories(${CMAKE_SOURCE_DIR}/third/ffmpeg/lib)
link_directories(${CMAKE_SOURCE_DIR}/third/opencv/lib)

set(OPENCV_LIBS opencv_imgproc opencv_highgui opencv_contrib opencv_core)
set(FFMPEG_LIBS avformat avfilter avcodec avutil postproc swresample swscale vpx x264 x265 fdk-aac mp3lame opus)
set(SYSTEM_LIBS ssl crypto pthread z bz2 m dl freetype)

file(GLOB_RECURSE sources src/*.[ch]pp)
list(REMOVE_ITEM sources ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

#编译成可执行文件
add_executable(${TARGET} ${sources} ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

#添加链接库
target_link_libraries(${TARGET} PUBLIC ${OPENCV_LIBS} ${FFMPEG_LIBS} ${SYSTEM_LIBS})
