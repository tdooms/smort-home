cmake_minimum_required(VERSION 3.10)
project(SHP)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5Core    REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Gui     REQUIRED)

file(GLOB_RECURSE HDRS ${PROJECT_SOURCE_DIR}/src/*.h  )
file(GLOB_RECURSE SRCS ${PROJECT_SOURCE_DIR}/src/*.cpp)

add_executable(SHP ${HDRS} ${SRCS})

target_include_directories(SHP PRIVATE ${PROJECT_SOURCE_DIR}/external)
target_include_directories(SHP PRIVATE ${PROJECT_SOURCE_DIR}/../dot/src)

find_package(Td 1.6.0 REQUIRED)
target_link_libraries(SHP PRIVATE Td::TdStatic Qt5::Widgets Qt5::Widgets Qt5::Gui)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wnon-virtual-dtor -Wold-style-cast -Wunused -Woverloaded-virtual -Wpedantic -Wnull-dereference -Wdouble-promotion")

