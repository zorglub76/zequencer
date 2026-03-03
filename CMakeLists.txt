# zequencer/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Tell ESP-IDF where our custom components live
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(zequencer)