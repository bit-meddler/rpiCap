#!/bin/bash

echo "This will do till I work out CMake."

echo "Cleaning up"
rm *.o
rm *.a
rm camServer

echo "Making helpers Lib"
g++ -Wall -std=c++17 -c camHelpers.cpp -o camHelpers.o
ar -rcvs camHelpers.a camHelpers.o

echo "Making the vision Lib"
g++ -Wall -std=c++17 -O3 -mfpu=neon -mtune=cortex-a53 -funsafe-math-optimizations -march=armv8-a+crc+simd -c rpiVision.cpp -o rpiVision.o
ar -rcvs rpiVision.a rpiVision.o

echo "Compiling the Camera Server"
g++ -Wall -std=c++17 -O1 -pthread camServer.cpp camHelpers.a -o camServer

