#
# CMakeLists.txt -- IAQ gateway (nRF52840 Dongle)
#
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(iaq_gateway)

target_sources(app PRIVATE src/main.c)
