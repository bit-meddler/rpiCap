cmake_minimum_required( VERSION 3.10 )
project( rpiCam )

set( CMAKE_CXX_STANDARD 17 )
# How to require pthread?
target_include_directories( rpiCam PUBLIC "${PROJECT_BINARY_DIR}" )

add_executable( rpiCam camServer.cpp )

add_library( camHelpers STATIC camHelpers.cpp )

find_library( CAM_HELPERS camHelpers )
# How to compile library?
# how to compile library with flags eg -mfpu=neon ?

target_link_libraries( rpiCam LINK_PUBLIC $(CAM_HELPERS) )
