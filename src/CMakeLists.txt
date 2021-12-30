cmake_minimum_required( VERSION 3.10 )
project( rpiCam )

set( CMAKE_CXX_STANDARD 17 )

# TODO: Globally, I probably want -O3 and maybe -mtune=cortex-a53 for release versions

target_include_directories( rpiCam PUBLIC "${PROJECT_BINARY_DIR}" )

add_executable( rpiCam camServer.cpp )

add_library( camHelpers STATIC camHelpers.h camHelpers.cpp )

# rpiVision uses neon, so requires extra compile switches
add_library( rpiVision STATIC rpiVision.h rpiVision.cpp )
set_target_properties( rpiVision PROPERTIES COMPILE_FLAGS "-O3 -mfpu=neon -mtune=cortex-a53 -funsafe-math-optimizations -march=armv8-a+crc+simd" )

# Cam server requires pthreads
set( THREADS_PREFER_PTHREAD_FLAG ON )
set( CMAKE_THREAD_PREFER_PTHREAD ON )

find_package( Threads REQUIRED )

target_link_libraries(  rpiCam

						PUBLIC
						camHelpers
						rpiVision
						
						PRIVATE
						Threads::Threads
)

# TODO: CPack and Install to sensible place.  Could this auto 'deamonize' the camServer as well?