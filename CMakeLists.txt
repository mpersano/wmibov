cmake_minimum_required(VERSION 3.2)

project(wmibov)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wno-write-strings")

find_package(CURL REQUIRED)
find_package(X11 REQUIRED)

include_directories(${CURL_INCLUDE_DIRS} ${X11_INCLUDE_DIR} ${X11_Xpm_INCLUDE_PATH})

set(SOURCES main.cc curl_request.cc)

add_executable(wmibov ${SOURCES})

target_link_libraries(wmibov ${CURL_LIBRARIES} ${X11_LIBRARIES} ${X11_Xpm_LIB})
